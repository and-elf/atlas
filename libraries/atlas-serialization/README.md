# atlas-serialization

**Status:** Seeded. Implements `atlas::serialization::ByteWriter` (`include/atlas/serialization/byte_writer.hpp`) and `atlas::serialization::ByteReader` (`include/atlas/serialization/byte_reader.hpp`) — a matched encode/decode pair for the fixed-width integer types (`u8`/`u16`/`u32`/`u64`, `i8`/`i16`/`i32`/`i64`) using an explicit, fixed little-endian byte order, rather than relying on native struct layout or `memcpy`. `ByteReader` fails a read explicitly (`std::nullopt`) rather than reading past the end of its backing buffer when insufficient bytes remain. Nothing else in this library's eventual scope (reflection-driven generic struct serialization, persistence/file-format framing, variable-width/varint encoding, floating-point encoding) is implemented yet.

**Scoping decisions:**
- Byte order is fixed at little-endian unconditionally — not host-endian-dependent — per spec §4: bit-exact determinism must hold "across different machines of the same target platform," and Atlas's currently-supported platforms (Linux x86-64, macOS ARM, Windows x86-64) are all little-endian today, but coding this as "whatever the host's native order happens to be" would be exactly the kind of implicit platform dependency §4 calls out, latent until a big-endian target ever appeared.
- Reflection-driven generic serialization of arbitrary structs is explicitly out of scope for this slice: `atlas-reflection` doesn't exist as a dependency yet, so any such attempt now would be premature and would have to be redone once it does.
- Signed integers are encoded as the two's complement bit pattern of their same-width unsigned counterpart. Converting that pattern back to a signed type on read is well-defined as of C++20 ([conv.integral]), so no manual sign-fixup logic is needed.
- `ByteWriter` and `ByteReader` are unrelated to (and do not depend on) `atlas::core::SemanticVersion`'s wire format or any future contract/reflection encoding — this library provides the byte-level primitive other libraries can build a structured format on top of, not a structured format itself.

**Provides:** serialization mechanisms, data encoding, persistence support.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§4 Architectural Invariants](../../docs/specification/04-architectural-invariants.md) (deterministic execution — the reason encoding is explicit-endianness rather than native-layout), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (dependency rules)

## Dependency position

`atlas-serialization` depends only on `atlas_project_options`/`atlas_project_warnings` and the standard library so far — no dependency on `atlas-core` or `atlas-entity` yet, since nothing in either is needed by the current scope. Per §5, it may depend on lower-level libraries and generated contracts as those needs arise, never upward on capabilities, applications, or editor/deployment-specific code.
