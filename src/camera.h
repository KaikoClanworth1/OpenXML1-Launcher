#pragma once
#include "pc_controls.h"

// "Flip camera left and right" without changing the game: the game turns the
// camera from the CameraLeft and CameraRight bindings (keys and controller), so
// swapping those two bindings for a player flips their turning. The in-game
// Advanced Options screen shows the swap, and can undo it. Mouse drag turning
// reads the mouse directly and is not affected.
namespace launcher {

inline bool camera_flipped(const Xml1PcSettings& s, unsigned player)
{
    const uint32_t left = s.pad_bindings[player][XML1_PC_CAMERA_LEFT], right = s.pad_bindings[player][XML1_PC_CAMERA_RIGHT];
    if (left == XML1_PAD_RX_POS || right == XML1_PAD_RX_NEG) return true;
    if (left == XML1_PAD_RX_NEG || right == XML1_PAD_RX_POS) return false;
    // No right-stick turning bound: judge by the default keys, L and J.
    return s.keys[player][XML1_PC_CAMERA_LEFT] == 'L' && s.keys[player][XML1_PC_CAMERA_RIGHT] == 'J';
}

inline void set_camera_flipped(Xml1PcSettings& s, bool flipped)
{
    auto swap = [](uint32_t (&table)[4][XML1_PC_ACTION_COUNT], unsigned player) {
        uint32_t left = table[player][XML1_PC_CAMERA_LEFT];
        table[player][XML1_PC_CAMERA_LEFT] = table[player][XML1_PC_CAMERA_RIGHT];
        table[player][XML1_PC_CAMERA_RIGHT] = left;
    };
    for (unsigned player = 0; player < 4; ++player) {
        if (camera_flipped(s, player) == flipped) continue;
        swap(s.keys, player);
        swap(s.alternate_keys, player);
        swap(s.pad_bindings, player);
        swap(s.alternate_pad_bindings, player);
    }
}

}
