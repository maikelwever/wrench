# Deadlocked Beta Gadget WAD Notes

`dvd/GADGET.WAD` contains a 0x248-byte WAD header followed by gadget class blobs.
Each WAD entry is:

```c
struct Entry {
	s32 class_id;
	s32 offset;
	s32 size;
};
```

The class IDs are written naturally as hex tags (`1087`, `20f9`, etc.).

## Class Blob

Each class blob begins with a simple two-section wrapper:

```c
struct DlBetaGadgetClassHeader {
	s32 version;        // always 3 in the samples
	s32 section_count;  // always 2 in the samples
	s32 sblk_offset;    // always 0x18
	s32 sblk_size;
	s32 data_offset;    // sblk_offset + sblk_size
	s32 data_size;      // reaches EOF
};
```

The first section is an `SBlk` block named `mn00`. The second section is native
PS2/VU render data. The native data is not a retail `MobyArmorHeader` or
`MobyPacketEntry` table, so Wrench's existing `meshonly` parser cannot read it
directly.

## SBlk Header

Offsets below are relative to the start of the `SBlk` section.

```c
struct SBlkHeader {
	u32 magic;                    // "SBlk"
	u32 version;                  // 3
	u32 kind;                     // 4
	u32 name;                     // "mn00"
	u32 flags_or_zero;            // 0 in samples
	u16 unknown_14;
	u16 upload_count;            // number of UploadDescriptor records
	u16 command_count;           // number of CommandDescriptor records
	u16 unknown_1a;
	u32 header_size;             // 0x40
	u32 command_table_offset;    // 0x40 + upload_count * 0xc
	u32 vu_base;                 // 0x5040 in all samples
	u32 data_size_a;             // same as class data_size
	u32 data_size_b;             // same as class data_size
	u32 unknown_30;
	u32 upload_data_offset;      // often points after descriptor tables
	u32 unknown_38;
	u32 unknown_3c;
};
```

Validation seen in every sample:

```text
command_table_offset == 0x40 + upload_count * 12
data_size_a == data_size_b == class data_size
vu_base == 0x5040
```

## Upload Descriptors

Immediately after the 0x40-byte `SBlk` header is the upload descriptor table:

```c
struct UploadDescriptor {
	u32 vu_addr_or_kind;
	u32 count_and_flags;
	u32 source_offset;
};
```

Common first fields are `0x78`, `0x7d`, `0x7e`, and `0x7f`. These look like
VU-memory destination addresses or upload kinds. The low bits of
`count_and_flags` behave like a qword count. Some samples set high bits such as
`0x10000` or `0x280000`, so this is not only a raw count.

Example from `20f9.bin`:

```text
vu/kind=0x7f count_flags=0x1 source=0x0
```

Example from `1087.bin`:

```text
vu/kind=0x7d count_flags=0x8 source=0x0
vu/kind=0x7f count_flags=0x1 source=0x40
vu/kind=0x7f count_flags=0x1 source=0x48
...
```

The upload source data follows the command table and contains GIF/VU constants.

## Command Descriptors

At `command_table_offset` is an 8-byte command descriptor table:

```c
struct CommandDescriptor {
	u32 opcode_and_param; // high byte opcode, low 24 bits parameter
	u32 arg;
};
```

Observed opcodes across the sample set:

```text
0x01: common draw/packet command. Param is usually 0, 0x18, 0x30, ...
0x04: less common draw/packet-like command.
0x14-0x18: state/control commands.
0x19-0x1f: material/GIF/render-state commands.
0x20,0x22,0x26,0x29,0x2b: uncommon state/control commands.
```

Opcode `0x01` dominates the stream and is the best candidate for a mesh packet
descriptor. Its low 24-bit parameter looks like a VU address or packet offset;
its second word is often a material/texture/state argument.

Example from `20f9.bin`:

```text
op=0x01 param=0x000000 arg=0
```

Example from `1087.bin`:

```text
op=0x01 param=0x000000 arg=0
op=0x01 param=0x000018 arg=0
op=0x1b param=0x000064 arg=5
op=0x01 param=0x000030 arg=0
op=0x01 param=0x000048 arg=0
op=0x01 param=0x000060 arg=5
...
```

## Native Data

The class data section is VU-ready render data, not a normal Wrench moby class
wrapper. Many qwords resemble packed vertex records, but the first 8 bytes do
not match retail `MOBY::MobyVertex` skin/matrix fields. This means a working
mesh exporter likely needs a beta-specific reader that:

1. Parses `SBlkHeader`, upload descriptors, and command descriptors.
2. Reconstructs the VU upload/render state used by opcode `0x01`.
3. Converts the beta qword vertex format to `GLTF::Mesh` directly.

The existing Wrench `MOBY::recover_packets` path should still be reusable after
the beta data is converted into `MOBY::MobyPacket`, but the descriptor tables
are not already in `MOBY::MobyPacketEntry` form.
