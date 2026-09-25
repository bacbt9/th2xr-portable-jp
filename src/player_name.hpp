#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace th2 {

struct PlayerName {
    std::string family;
    std::string given;
    std::string family_reading;
    std::string given_reading;
    std::string nickname;
    std::string nickname_reading;
};

// Longest name field the original name dialog accepts: 13-byte buffers hold
// six full-width Shift_JIS characters plus the terminator.
inline constexpr std::size_t max_player_name_characters = 6;

// DEF_NAME_* from the original GM_AvgMsg.h.
PlayerName load_default_player_name();

// The original DefaultCharName: the five dialog-editable fields all match the
// defaults (the nickname reading is never edited, so it is not compared).
bool uses_default_voice_name(
    const PlayerName& name, const PlayerName& default_name);

// Name dialog validation (NameDialogBoxProc). Returns an empty string when the
// name is acceptable, otherwise the Japanese error message to show.
std::string validate_player_name(const PlayerName& name);

// AVG_SetName: expands *h2, *nnk, *nlk, *nfk, *nn, *nl, *nf (optionally
// followed by a 1-6 character index).
std::string substitute_player_name(
    std::string_view source, const PlayerName& name,
    bool use_komaki_given_name = false);

}  // namespace th2
