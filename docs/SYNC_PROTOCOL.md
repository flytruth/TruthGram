# TruthGram Sync Protocol (Saved Messages)

TruthGram has **no external cloud**. Per-device state (enabled plugins, theme
accent, ghost-mode toggles, voice-changer preset, screen-shield state) is
synchronized across the user's own TruthGram instances by posting an encrypted
packet into the user's **Saved Messages** peer. Other TruthGram instances
detect the magic header and re-import the state. The packet is invisible to
other Telegram clients (it is posted as a long text message that upstream
clients render as a single opaque line; the TruthGram UI hides it from the
chat list entirely).

## Envelope

```
TGSYNC:v1:<b64( header(12) || ciphertext(N) || tag(16) )>
```

- Magic ASCII prefix: `TGSYNC:v1:` — non-encrypted, used for fast scan.
- `header` (12 bytes): 4-byte device-id, 4-byte sequence, 4-byte payload-type.
- `ciphertext`: AES-256-GCM of the protobuf-serialized `SyncPacket`.
- `tag`: 16-byte GCM authentication tag.

The 256-bit AES key is derived from the user's MTProto auth key via HKDF-SHA256
with the static info string `"truthgram.cloud_sync.v1"`. Because the auth key
never leaves the device and is account-scoped, only instances logged into the
same account can decrypt the packet — exactly the trust boundary we want.

## SyncPacket (protobuf schema)

```proto
syntax = "proto3";
package truthgram.sync.v1;

message PluginEntry {
  string id = 1;
  string version = 2;
  bool enabled = 3;
  bytes settings = 4;        // opaque, plugin-defined
}

message ThemeOverride {
  string theme_name = 1;
  uint32 accent_rgb = 2;     // 0xRRGGBB
  uint32 link_rgb = 3;
  bool frosted_glass = 4;
}

message GhostState {
  bool suppress_read_history = 1;
  bool suppress_read_contents = 2;
  bool suppress_typing = 3;
  bool suppress_presence = 4;
}

message VoicePreset {
  string name = 1;
  float pitch_semitones = 2;
  float reverb_mix = 3;
  // ... DSP params, see voice_changer.h
}

message SyncPacket {
  uint32 schema_version = 1;
  uint64 timestamp_ms = 2;
  string device_label = 3;
  repeated PluginEntry plugins = 4;
  ThemeOverride theme = 5;
  GhostState ghost = 6;
  VoicePreset voice = 7;
  bool screen_shield_streamer_mode = 8;
}
```

## Conflict resolution

Last-writer-wins by `timestamp_ms`, except for plugin `settings` blobs which
are **merged per-plugin-id** (the newest settings blob for each id wins
independently). The merge result is itself re-published as a new packet so
every instance converges within one round trip.

## Throttle & chunking

- A device publishes at most **once per 30 seconds** (debounced) and only if
  local state actually changed (content hash compare).
- A `SyncPacket` larger than 3.5 KB (after GCM) is chunked: each chunk carries
  `(chunk_index, chunk_total)` in the 4-byte payload-type field and is posted
  as a separate Saved Messages message; the importer reassembles in order and
  verifies the GCM tag only after the last chunk arrives.

## Privacy

- No `SyncPacket` ever contains message text, peer ids, or contact data — only
  TruthGram's own configuration.
- The publish path uses the same MTProto transport as any normal message; no
  extra endpoint is contacted.
- Users can disable sync entirely in `Settings > TruthGram > Sync`; when
  disabled, the device still *reads* incoming packets (so other instances can
  push to it) but never *writes* them.
