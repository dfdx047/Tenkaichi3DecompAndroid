#pragma once
#include <array>
#include <bitset>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// PCSX2-compatible texture names (docs/plans/graphics/HD_TEXTURES_PLAN.md): the name a texture
// replacement pack made with PCSX2 gives each texture the game draws with. Follows the GS register
// writes and image transfers of the GIF stream with its own copy of GS memory and of the CLUT, and
// names a texture the first time it is drawn after its data, palette or registers changed.
//
// A name is "<texture hash>-<palette hash>-<format word>" (no palette part without a palette), in
// lowercase hexadecimal without leading zeros except the eight-digit format word:
// - texture hash: XXH3-64 over the texture's 256-byte GS blocks in block order (rows of blocks, each
//   left to right); a texture smaller than one block, or a format that does not fill all 32 bits of a
//   pixel, is hashed as read out instead: one byte per pixel (the index) for palette formats, four
//   (RGBA, 24-bit with TEXA's alpha) otherwise, row by row;
// - palette hash: XXH3-64 over the palette as 32-bit entries (16 for 4-bit, 256 for 8-bit), as read at
//   CSA; packs made with older PCSX2 versions (pack 2) mostly hash only the entries from the lowest to the
//   highest index the texture uses (found by Tenkaichi3Decomp on its game's pack), so a name has both;
// - format word: PSM, TW << 6, TH << 10, and for non-palette formats below 32 bits TEXA's TA0 << 15,
//   AEM << 23, TA1 << 24.
// Behaviour adapted from PCSX2's texture replacement (pinned GSTextureCache.cpp, GSClut.cpp); GS
// memory layouts from the public GS layout as in gs_command_sink.h.
class GsTextureNames {
public:
    GsTextureNames():memory_(1024*1024) {regs_[0x1A]=1;} // PRMODECONT starts at 1: PRIM's attributes apply.
    // A GS register write as the GIF delivers it (A+D, REGLIST, decoded PACKED).
    void reg(unsigned reg,std::uint64_t value);
    // IMAGE data of the host->local transfer in progress.
    void image(std::span<const std::uint8_t> data);

    struct Name {
        std::uint64_t texture=0,palette=0,palette_used=0;std::uint32_t format=0;bool has_palette=false;
        std::string text() const;     // With the whole palette's hash.
        std::string textUsed() const; // With the used entries' hash (the same as text() without a palette).
    };
    // The name of a texture as TEX0 and TEXA describe it, from the current memory and CLUT; none for a
    // format this does not name (counted).
    std::optional<Name> name(std::uint64_t tex0,std::uint64_t texa);

    // Names drawn with ("<used entries' name>/<whole palette's name>" for palette textures), and how many
    // times each was named (a texture is named again after a change).
    // Counted only with count_drawn (the naming log); the renderer asks for names itself when it caches
    // a texture, so drawing with HD textures needs no naming per draw.
    std::map<std::string,unsigned> drawn;
    bool count_drawn=true;
    // What the copy does not follow: image transfers in formats it does not write, transfers in other
    // directions or orders, CLUT loads in formats it does not read, textures in formats it does not name.
    unsigned unsupported_uploads=0,local_copies=0,unsupported_cluts=0,unsupported_textures=0;
    std::map<unsigned,unsigned> unsupported_formats; // Draws of textures not named, by PSM.
    std::uint64_t uploaded_pixels=0;
    // Uploads and CLUT loads that repeated the last one into unchanged memory, so nothing changed.
    std::uint64_t uploads=0,clut_loads=0,repeated_uploads=0,repeated_cluts=0;

    std::vector<std::uint32_t>& memory() {return memory_;}

    // Palettes the game draws on the GPU (the fighters' lit palettes: each frame it draws the whole
    // palette page from base palettes it uploads): only the renderer holds them, and PCSX2 names such a
    // texture with the palette as drawn, read back from its render target. Pages CLUT loads read are
    // watched; a primitive covering one marks it drawn. The renderer copies those pages after each frame
    // without waiting for them (drawnPalettePages, takeDrawnPage); a CLUT load from a drawn page reads the
    // latest copy, a frame or two old. The game relights slowly, so successive frames' pages are almost
    // always the same, and this names as exactly as a read that waits (measured: the same share of draws).
    std::vector<unsigned> drawnPalettePages() const;
    void takeDrawnPage(unsigned page,const std::uint32_t* words); // The page's 2048 words.
    // GS addresses as gs_command_sink.h computes them (word, byte, nibble), from a table of each page's
    // layout: the same results at a fraction of the cost per pixel.
    static std::size_t ct32Word(unsigned bp,unsigned bw,unsigned x,unsigned y);
    static std::size_t t8Byte(unsigned bp,unsigned bw,unsigned x,unsigned y);
    static std::size_t t4Nibble(unsigned bp,unsigned bw,unsigned x,unsigned y);
    std::array<std::uint16_t,512>& clut() {return clut_;}

private:
    std::vector<std::uint32_t> memory_; // GS memory, 4 MB, by word address.
    std::array<std::uint16_t,512> clut_{}; // The CLUT buffer: 32-bit entry i as halves i and i + 256.
    std::array<std::uint64_t,0x64> regs_{};
    std::uint32_t cbp_[2]{};
    // Host->local transfer in progress.
    bool transfer_=false;unsigned psm_=0,bp_=0,bw_=0,x0_=0,y0_=0,w_=0,h_=0,x_=0,y_=0;
    std::uint32_t partial_=0;unsigned partial_bytes_=0;
    // A transfer's data is kept until it is complete: the game sends the same textures to the same place
    // again and again (every frame in fights), and a transfer equal to the last one into its pages, which
    // nothing wrote since, leaves memory as it is, so it is not written and keeps the hashes valid.
    std::vector<std::uint8_t> pending_;std::size_t transfer_bytes_=0;bool direct_=false;
    std::array<std::uint64_t,3> upload_key_{};
    struct Upload {std::uint64_t hash,generation;std::vector<std::uint16_t> pages;};
    struct UploadKeyHash {std::size_t operator()(const std::array<std::uint64_t,3>& k) const {return std::size_t((k[0]*0x9E3779B97F4A7C15ull)^(k[1]*0xC2B2AE3D27D4EB4Full)^k[2]);}};
    std::unordered_map<std::array<std::uint64_t,3>,Upload,UploadKeyHash> uploads_;
    std::vector<std::uint16_t> written_pages_; // Pages the current transfer wrote.
    // The last CLUT load: its TEX0 fields and the generation it was read at.
    std::uint64_t clut_key_=~0ull,clut_generation_=0;
    // Changes since a context's texture was last named.
    std::uint64_t generation_=0;
    std::uint64_t named_generation_[2]{~0ull,~0ull},named_tex0_[2]{},named_texa_[2]{};
    // Texture hashes are kept until a transfer writes one of the 8 KB pages the texture lies in (the
    // renderer asks again each time it caches a texture, which in fights is many times per frame).
    std::array<std::uint64_t,512> page_written_{}; // The generation of the last transfer into each page.
    std::uint64_t all_written_=0;                  // A transfer this does not follow: every page.
    struct Hashed {std::uint64_t hash,generation;unsigned lowest,highest;}; // With the indices a palette texture uses.
    struct KeyHash {std::size_t operator()(const std::pair<std::uint64_t,std::uint64_t>& k) const {return std::size_t(k.first*0x9E3779B97F4A7C15ull^k.second);}};
    std::unordered_map<std::pair<std::uint64_t,std::uint64_t>,Hashed,KeyHash> hashed_;
    std::uint64_t textureHash(std::uint64_t tex0,std::uint64_t texa,unsigned* lowest=nullptr,unsigned* highest=nullptr);

    void startTransfer(std::uint64_t dir);
    void finishUpload();
    void writePending(); // Writes the data kept so far; the rest of the transfer is written as it comes.
    void writeBytes(std::span<const std::uint8_t> data);
    void loadClut(std::uint64_t tex0);
    void kick();
    // Drawn palettes: the pages CLUT loads read and those the renderer's copies hold, the generation of the
    // last draw over each page, the last vertices (a primitive's area), the copies.
    std::bitset<512> clut_pages_,copied_;
    std::array<std::uint64_t,512> page_drawn_{};
    std::array<std::uint64_t,3> vertices_{};unsigned vertex_count_=0;
    std::vector<std::uint32_t> drawn_copy_;
    void vertex(std::uint64_t xyz);
    bool pageDrawn(unsigned page) const {return page_drawn_[page]>page_written_[page] && page_drawn_[page]>all_written_;}
};
