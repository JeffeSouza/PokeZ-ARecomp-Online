# Native mod updates for launcher 0.1.34

Keep the existing UDP update negotiation and NSO chunk messages. The update file
may now carry reviewed compatibility data after the complete original NSO. No
new opcode, chunk format, command, player slot, or server-clock change is needed.

The launcher accepts a new native build ID without a launcher rebuild when the
download includes matching, validated compatibility data. This is metadata for
the exact build, not a wildcard admission or a promise that arbitrary native
changes preserve the runtime ABI.

## Produce the update

Use the original NSO and a reviewed JSON profile derived from that SAME build's
ELF/CFG and actual instruction bytes:

```text
python tools/lumiose/pack_mod_update.py --nso build/subsdk9 --compatibility-profile build/compatibility.json --out release/subsdk9.update
```

The packer calls the launcher's `native_compat.validate_candidate` with the same
`tools/nso.py` parser, before creating output. It checks the exact BASE SHA256,
full build ID, import relocations, executable camera return sites, relative
switch-table targets and qualified function-entry union, and byte witnesses.
It also verifies its output using the launcher's trailer reader. Existing
output is refused. The input NSO/profile are not modified.

The profile schema is `lumiose-native-compatibility-v1`, `abi_revision: 1`:

- `build_id`: 40 hex characters for a 20-byte ID with 12 zero padding bytes,
  or 64 for a genuine 32-byte ID; `build_id32`: the exact full 32-byte NSO ID.
  Trailing zero bytes inside the identity are preserved.
- `sha256`: SHA256 of the original BASE NSO, excluding metadata/footer.
- `camera_query_returns`: three qualified native instruction offsets.
- `switch_tables`: `[offset, count, format, anchor]` rows derived from actual CFG.
- `additional_function_entries`: the qualified switch-target/entry union.
- `witnesses`: `[offset, hexBytes]` pairs for reviewed native semantics.
- `name_preservation_verified: true`: an EXISTING independent review result.
- Optional `version`: the actual build's supported version field.

The packer never derives or invents these values and never grants the name
preservation attestation. Do not copy another build's offsets, rename a profile
to a new ID, or set the attestation merely to pass the tool. Retain the build's
source/ELF derivation and review alongside the JSON.

## Exact file format

```text
BASE_NSO || UTF8_JSON || little_endian_u64(BASE length)
                    || little_endian_u32(JSON length)
                    || 16-byte "LUMIOSE-COMPAT1\0"
```

JSON is at most 1 MiB. BASE bytes and their NSO header remain unchanged. Serve
the ENTIRE packed file through the existing chunk transport; use the packer's
`download_length` and `download_sha256` for any whole-download size/hash fields.
`base_length` and `base_sha256` describe the normalized native module instead.
Do not strip the trailer on the server or use the BASE hash for the packed blob.
The launcher validates metadata and strips it before native preparation.

## Legacy and failure behavior

Existing raw NSOs with built-in profiles or supported legacy compatibility still
use their original path. Unknown native builds without matching metadata fail
with an actionable compatibility error; publish a reviewed exact-build profile
instead of downgrading the module, disabling byte checks, or accepting an
unreviewed fallback. A malformed or mismatched trailer is refused.

The 0.1.34 full bundle must retain the user's active **0124 / 1.5.8.8** mod.
The prepared **0f595e** profile is only a packaging/round-trip fixture. Do not
replace the bundled 0124 NSO with it or label that private capture build as the
active release. This handoff changes no deployed relay or installed files.
