/*
	wrench - A set of modding tools for the Ratchet & Clank PS2 games.
	Copyright (C) 2019-2022 chaoticgd

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <wrenchbuild/asset_unpacker.h>

packed_struct(DlBetaGadgetEntry,
	s32 class_id;
	ByteRange range;
)

packed_struct(DlBetaGadgetWadHeader,
	/* 0x000 */ s32 header_size;
	/* 0x004 */ Sector32 sector;
	/* 0x008 */ DlBetaGadgetEntry gadgets[48];
)
static_assert(sizeof(DlBetaGadgetWadHeader) == 0x248);

static void unpack_dl_beta_gadget_wad(
	GadgetWadAsset& dest, const DlBetaGadgetWadHeader& header, InputStream& src, BuildConfig config);

on_load(Gadget, []() {
	GadgetWadAsset::funcs.unpack_dl = wrap_wad_unpacker_func<GadgetWadAsset, DlBetaGadgetWadHeader>(
		unpack_dl_beta_gadget_wad, false);
})

static void unpack_dl_beta_gadget_wad(
	GadgetWadAsset& dest, const DlBetaGadgetWadHeader& header, InputStream& src, BuildConfig config)
{
	CollectionAsset& gadgets = dest.gadgets(SWITCH_FILES);
	
	for (const DlBetaGadgetEntry& entry : header.gadgets) {
		if (entry.class_id < 0 || entry.range.empty()) {
			continue;
		}
		
		verify(entry.range.offset >= header.header_size,
			"Invalid dlbeta gadget WAD entry for class %04x: offset is inside the header.",
			entry.class_id);
		verify(entry.range.offset + entry.range.size <= src.size(),
			"Invalid dlbeta gadget WAD entry for class %04x: range extends past the end of the file.",
			entry.class_id);
		
		std::string tag = stringf("%04x", entry.class_id);
		MobyClassAsset& gadget = gadgets.foreign_child<MobyClassAsset>(tag + "/" + tag, false, tag);
		gadget.set_id(entry.class_id);
		unpack_asset(gadget, src, entry.range, config, "dlbeta_gadget");
	}
}
