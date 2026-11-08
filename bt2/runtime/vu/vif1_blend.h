// In-between frames for 60 fps battles (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, way 4): the VIF1
// list of a frame with the values that move between two frames set in between. Two successive fight
// frames send mostly the same codes in the same order; what differs is data: the camera's and the bones'
// matrices in V4-32 unpacks (and DIRECT data, taken from the newer frame). A few pieces of geometry
// change size or come and go from frame to frame (7,000 to 8,000 unpacks a frame, a fight's lists paired
// whole in fewer than half the frames), so the unpacks of the two lists are aligned like lines in a diff
// (Myers' algorithm), each unpack known by its code (address, count, format). Floats that differ in an
// aligned pair are blended; an unpack only the newer list has, and words that are not normal floats
// (tags, integers), are taken from the newer frame as they are.
// BT2's matrices (docs/plans/graphics/HIGH_FRAME_RATE_PLAN.md, step 1): a 34-quadword unpack holds two
// bone matrices (quadwords 22 to 25 and 26 to 29: three axes, then a position), a 12-quadword unpack the
// camera (0 to 3 view-projection, 4 to 7 view). Their axes are blended as turning, not shrinking: each
// blended axis keeps the blended length of its two originals (as Tenkaichi3Decomp's in-between pictures
// do); positions are blended linearly. A camera whose view axes turn by more than 25 degrees between the
// two frames is a cut: the lists are not blended (no in-between picture across it).
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

struct Vif1Unpack {std::size_t start=0,words=0;unsigned format=0;std::uint32_t code=0;}; // format: the UNPACK code's bits 0 to 3 (VN, VL).

// The data of each UNPACK of a list, in order; nullopt when the list cannot be walked (a code this does
// not size, or data past the end).
std::optional<std::vector<Vif1Unpack>> vif1Unpacks(std::span<const std::uint32_t> words);

// `newer` with each float that differs from `older` in an aligned pair of V4-32 unpacks set to
// older + t (newer - older); nullopt when a list cannot be walked or the two differ by more than
// `max_edits` unpacks (not the same scene). `blended` counts the words changed, `unmatched` the newer
// list's unpacks without a partner.
std::optional<std::vector<std::uint32_t>> blendVif1(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,float t,
                                                    std::size_t* blended=nullptr,std::size_t* unmatched=nullptr,std::size_t max_edits=1024);

// The smallest cosine between a camera view axis of `older` and the same axis of `newer` over their
// aligned 12-quadword unpacks (1 when there is none): below cos 25 degrees the camera cut.
float cameraTurn(std::span<const std::uint32_t> older,std::span<const std::uint32_t> newer,std::size_t max_edits=1024);

// The aligned pairs of two sequences: index pairs (i, j) with a[i] == b[j], in order, of a shortest
// edit script (Myers); nullopt when it needs more than `max_edits` insertions and deletions.
std::optional<std::vector<std::pair<std::size_t,std::size_t>>> alignSequences(std::span<const std::uint64_t> a,std::span<const std::uint64_t> b,std::size_t max_edits);
