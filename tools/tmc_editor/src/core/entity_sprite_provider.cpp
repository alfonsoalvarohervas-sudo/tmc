#include "core/entity_sprite_provider.hpp"
#include "core/rom_data_reader.hpp"
#include <cstring>
#include <algorithm>

namespace tmc {


EntitySpriteProvider& EntitySpriteProvider::get() {
    static EntitySpriteProvider instance;
    return instance;
}

EntitySpriteProvider::EntitySpriteProvider() = default;

EntitySpriteProvider::~EntitySpriteProvider() {
    clearCache();
}

void EntitySpriteProvider::clearCache() {
    for (auto& [k, tex] : m_textureCache) {
        if (tex) {
            SDL_DestroyTexture(tex);
        }
    }
    m_textureCache.clear();
}
SpriteInfo EntitySpriteProvider::getVanillaSpriteInfo(u8 kind, u8 id) const {
    // Kind 1: Player (Link)
    if (kind == 1) return { 1, 0x16, 0 };

    // Kind 3: Enemies
    if (kind == 3) {
        switch (id) {
            case 0: return { 174, 0, 1 };
            case 1: return { 161, 28, 3 };
            case 2: return { 236, 175, 2 };
            case 3: return { 169, 32, 5 };
            case 4: return { 171, 42, 8 };
            case 5: return { 98, 33, 16416 };
            case 6: return { 345, 5, 259 };
            case 7: return { 165, 0, 11 };
            case 8: return { 177, 112, 7 };
            case 9: return { 178, 0, 14 };
            case 10: return { 180, 30, 16 };
            case 11: return { 181, 35, 17 };
            case 12: return { 495, 0, 463 };
            case 13: return { 484, 42, 26 };
            case 14: return { 0, 0, 0 };
            case 15: return { 187, 34, 31 };
            case 16: return { 189, 36, 33 };
            case 17: return { 191, 38, 34 };
            case 18: return { 192, 40, 35 };
            case 19: return { 200, 43, 62 };
            case 20: return { 205, 111, 69 };
            case 21: return { 206, 111, 71 };
            case 22: return { 208, 110, 114 };
            case 23: return { 323, 4, 115 };
            case 24: return { 102, 119, 16400 };
            case 25: return { 215, 1, 134 };
            case 26: return { 216, 0, 135 };
            case 27: return { 219, 125, 140 };
            case 28: return { 220, 0, 141 };
            case 29: return { 221, 112, 143 };
            case 30: return { 222, 113, 144 };
            case 31: return { 483, 1, 145 };
            case 32: return { 223, 112, 146 };
            case 33: return { 224, 0, 168 };
            case 34: return { 225, 112, 169 };
            case 35: return { 216, 1, 135 };
            case 36: return { 226, 201, 173 };
            case 37: return { 286, 320, 421 };
            case 38: return { 228, 112, 199 };
            case 39: return { 229, 111, 215 };
            case 40: return { 230, 0, 216 };
            case 41: return { 231, 113, 217 };
            case 42: return { 235, 170, 222 };
            case 43: return { 238, 178, 225 };
            case 44: return { 237, 111, 224 };
            case 45: return { 239, 38, 227 };
            case 46: return { 240, 179, 228 };
            case 47: return { 483, 2, 145 };
            case 48: return { 104, 198, 16400 };
            case 49: return { 241, 1, 236 };
            case 50: return { 242, 200, 237 };
            case 51: return { 245, 112, 253 };
            case 52: return { 246, 0, 254 };
            case 53: return { 247, 112, 255 };
            case 54: return { 248, 229, 256 };
            case 55: return { 252, 309, 257 };
            case 56: return { 105, 0, 16432 };
            case 57: return { 254, 239, 264 };
            case 58: return { 167, 1, 33008 };
            case 59: return { 109, 1, 16416 };
            case 60: return { 174, 378, 1 };
            case 61: return { 228, 378, 199 };
            case 62: return { 237, 378, 224 };
            case 63: return { 262, 112, 353 };
            case 64: return { 106, 288, 16400 };
            case 65: return { 148, 0, 67 };
            case 66: return { 108, 1, 16416 };
            case 67: return { 263, 0, 366 };
            case 68: return { 249, 218, 256 };
            case 69: return { 241, 0, 236 };
            case 70: return { 207, 111, 72 };
            case 71: return { 110, 299, 16400 };
            case 72: return { 111, 0, 16400 };
            case 73: return { 0, 0, 0 };
            case 74: return { 112, 312, 16416 };
            case 75: return { 277, 308, 393 };
            case 76: return { 125, 112, 16400 };
            case 77: return { 125, 112, 16400 };
            case 78: return { 279, 314, 406 };
            case 79: return { 117, 319, 16416 };
            case 80: return { 279, 314, 406 };
            case 81: return { 122, 320, 16448 };
            case 82: return { 121, 320, 16400 };
            case 83: return { 491, 0, 423 };
            case 84: return { 116, 313, 16400 };
            case 85: return { 174, 0, 1 };
            case 86: return { 294, 113, 422 };
            case 87: return { 294, 113, 422 };
            case 88: return { 301, 0, 444 };
            case 89: return { 301, 0, 444 };
            case 90: return { 297, 315, 408 };
            case 91: return { 208, 110, 114 };
            case 92: return { 0, 0, 0 };
            case 93: return { 126, 338, 16448 };
            case 94: return { 295, 352, 485 };
            case 95: return { 283, 320, 418 };
            case 96: return { 313, 0, 498 };
            case 97: return { 312, 377, 499 };
            case 98: return { 311, 376, 500 };
            case 99: return { 315, 372, 504 };
            case 100: return { 126, 338, 16448 };
            case 101: return { 0, 0, 0 };
            default: return { 161, 28, 3 }; // Default to Chuchu
        }
    }

    // Kind 7: NPCs
    if (kind == 7) {
        switch (id) {
            case 0: return { 0, 0, 0 };
            case 1: return { 24, 49, 1 };
            case 2: return { 25, 50, 1 };
            case 3: return { 31, 48, 1 };
            case 4: return { 52, 0, 1 };
            case 5: return { 42, 54, 1 };
            case 6: return { { 57, 62, 1, 0, 0, 0 }, { 58, 62, 1, 0, 0, 0 } }; // Townsperson (Body + Head)
            case 7: return { { 59, 58, 1, 0, 0, 0 }, { 59, 58, 1, 25, 0, 0 } }; // Kid (Body + Head)
            case 8: return { { 43, 0, 2, 0, 0, 0 }, { 43, 0, 2, 25, 0, 0 } }; // Guard (Body + Head)
            case 9: return { 0, 0, 0 };
            case 10: return { 143, 65, 59 };
            case 11: return { 47, 70, 1, 16 }; // Maid (Full Frame 16)
            case 12: return { 144, 66, 60 };
            case 13: return { 146, 57, 61 };
            case 14: return { 147, 56, 64 };
            case 15: return { 37, 284, 2 };
            case 16: return { 60, 0, 1 };
            case 17: return { { 49, 62, 1, 0, 0, 0 }, { 49, 62, 1, 8, 0, 0 } }; // Beedle (Body + Head)
            case 18: return { 50, 63, 1 };
            case 19: return { { 61, 63, 1, 0, 0, 0 }, { 61, 63, 1, 7, 0, 0 } }; // Sitting Person (Body + Head)
            case 20: return { 51, 62, 1 };
            case 21: return { { 43, 55, 2, 0, 0, 0 }, { 43, 55, 2, 25, 0, 0 } }; // Guard 1 (Body + Head)
            case 22: return { 47, 70, 1, 16 }; // Maid 1 (Full Frame 16)
            case 23: return { 39, 67, 1 };
            case 24: return { 40, 68, 1 };
            case 25: return { 41, 69, 1 };
            case 26: return { 140, 59, 56 };
            case 27: return { 48, 71, 1 };
            case 28: return { 145, 53, 66 };
            case 29: return { 139, 75, 68 };
            case 30: return { 53, 76, 1 };
            case 31: return { 54, 1, 1 };
            case 32: return { 55, 1, 1 };
            case 33: return { 88, 79, 1 };
            case 34: return { 56, 77, 2 };
            case 35: return { 31, 48, 1 };
            case 36: return { 44, 55, 1 };
            case 37: return { 45, 55, 1 };
            case 38: return { 46, 55, 1 };
            case 39: return { 328, 80, 1 };
            case 40: return { 42, 54, 1 };
            case 41: return { 62, 81, 1 };
            case 42: return { 63, 81, 2 };
            case 43: return { 160, 5, 200 };
            case 44: return { 65, 1, 1 };
            case 45: return { 27, 180, 2 };
            case 46: return { 42, 54, 1 };
            case 47: return { 26, 181, 2 };
            case 48: return { 149, 183, 231 };
            case 49: return { 90, 191, 1 };
            case 50: return { 93, 192, 1 };
            case 51: return { 91, 192, 2 };
            case 52: return { 64, 193, 1 };
            case 53: return { 67, 82, 1 };
            case 54: return { 70, 0, 2 };
            case 55: return { 151, 233, 261 };
            case 56: return { 29, 234, 1 };
            case 57: return { 30, 281, 2 };
            case 58: return { 71, 246, 1 };
            case 59: return { 112, 312, 2 };
            case 60: return { 207, 111, 72 };
            case 61: return { 141, 60, 57 };
            case 62: return { 72, 247, 2 };
            case 63: return { 152, 249, 288 };
            case 64: return { 74, 251, 1 };
            case 65: return { 154, 252, 290 };
            case 66: return { 75, 253, 1 };
            case 67: return { 76, 254, 1 };
            case 68: return { 77, 255, 1 };
            case 69: return { 156, 256, 292 };
            case 70: return { { 78, 257, 1, 0, 0, 0 }, { 58, 257, 1, 0, 0, 0 } }; // Sitting Townsperson (Body + Head)
            case 71: return { 157, 258, 293 };
            case 72: return { 79, 259, 1 };
            case 73: return { 81, 260, 1 };
            case 74: return { 158, 265, 297 };
            case 75: return { 87, 62, 1 };
            case 76: return { 0, 0, 0 };
            case 77: return { 11, 22, 1 };
            case 78: return { 0, 0, 0 };
            case 79: return { 95, 0, 1 };
            case 80: return { 306, 0, 464 };
            case 81: return { 307, 0, 464 };
            case 82: return { 0, 0, 0 };
            case 83: return { 314, 234, 496, 1 }; // Hurdy Gurdy Man (Full Frame 1)
            case 84: return { 89, 355, 2 };
            case 85: return { 148, 0, 67 };
            case 86: return { 494, 0, 463 };
            case 87: return { 495, 0, 463 };
            case 88: return { 0, 0, 0 };
            case 89: return { 393, 88, 348 };
            case 90: return { 394, 88, 348 };
            case 91: return { 97, 0, 1 };
            case 92: return { 0, 0, 0 };
            case 93: return { 0, 0, 0 };
            case 94: return { 0, 0, 0 };
            case 95: return { 0, 0, 0 };
            case 96: return { 0, 0, 0 };
            case 97: return { 0, 0, 0 };
            case 98: return { 0, 0, 0 };
            case 99: return { 0, 0, 0 };
            case 100: return { 0, 0, 0 };
            case 101: return { 0, 0, 0 };
            case 102: return { 0, 0, 0 };
            case 103: return { 0, 0, 0 };
            case 104: return { 0, 0, 0 };
            case 105: return { 0, 0, 0 };
            case 106: return { 0, 0, 0 };
            case 107: return { 0, 0, 0 };
            case 108: return { 0, 0, 0 };
            case 109: return { 0, 0, 0 };
            case 110: return { 0, 0, 0 };
            case 111: return { 0, 0, 0 };
            case 112: return { 0, 0, 0 };
            case 113: return { 0, 0, 0 };
            case 114: return { 0, 0, 0 };
            case 115: return { 0, 0, 0 };
            case 116: return { 0, 0, 0 };
            case 117: return { 0, 0, 0 };
            case 118: return { 0, 0, 0 };
            case 119: return { 0, 0, 0 };
            case 120: return { 0, 0, 0 };
            case 121: return { 0, 0, 0 };
            case 122: return { 0, 0, 0 };
            case 123: return { 0, 0, 0 };
            case 124: return { 0, 0, 0 };
            case 125: return { 0, 0, 0 };
            case 126: return { 0, 0, 0 };
            case 127: return { 0, 0, 0 };
            case 128: return { 0, 0, 0 };
            case 129: return { 0, 0, 0 };
            case 130: return { 0, 0, 0 };
            default: return { 24, 48, 0 }; // Default to Gentari
        }
    }

    // Kind 6: Objects
    if (kind == 6) {
        switch (id) {
            case 0: return { 323, 0, 0 };
            case 1: return { 168, 0, 96 };
            case 2: return { 323, 0, 0 };
            case 3: return { 0, 0, 0 };
            case 4: return { 10, 0, 0 };
            case 5: return { 167, 1, 240 };
            case 6: return { 11, 0, 0 };
            case 7: return { 468, 5, 12 };
            case 8: return { 476, 10, 0 };
            case 9: return { 477, 10, 0 };
            case 10: return { 202, 1, 0 };
            case 11: return { 0, 0, 0 };
            case 12: return { 323, 0, 0 };
            case 13: return { 184, 1, 25 };
            case 14: return { 172, 3, 9 };
            case 15: return { 0, 0, 0 };
            case 16: return { 162, 0, 59 };
            case 17: return { 167, 171, 101 };
            case 18: return { 167, 2, 232 };
            case 19: return { 463, 14, 346 };
            case 20: return { 464, 14, 346 };
            case 21: return { 170, 0, 121 };
            case 22: return { 167, 2, 13 };
            case 23: return { 0, 0, 0 };
            case 24: return { 466, 1, 260 };
            case 25: return { 467, 1, 260 };
            case 26: return { 162, 3, 89 };
            case 27: return { 261, 1, 347 };
            case 28: return { 346, 47, 41 };
            case 29: return { 0, 0, 0 };
            case 30: return { 227, 148, 196 };
            case 31: return { 347, 0, 43 };
            case 32: return { 347, 47, 42 };
            case 33: return { 348, 47, 42 };
            case 34: return { 183, 0, 81 };
            case 35: return { 167, 0, 0 };
            case 36: return { 162, 0, 96 };
            case 37: return { 166, 4, 0 };
            case 38: return { 405, 108, 381 };
            case 39: return { 253, 0, 258 };
            case 40: return { 182, 0, 22 };
            case 41: return { 185, 29, 23 };
            case 42: return { 458, 116, 27 };
            case 43: return { 186, 5, 0 };
            case 44: return { 465, 296, 355 };
            case 45: return { 199, 1, 435 };
            case 46: return { 173, 0, 38 };
            case 47: return { 167, 46, 13 };
            case 48: return { 128, 109, 0 };
            case 49: return { 162, 3, 154 };
            case 50: return { 193, 5, 37 };
            case 51: return { 0, 0, 0 };
            case 52: return { 322, 0, 34 };
            case 53: return { 473, 310, 402 };
            case 54: return { 323, 0, 34 };
            case 55: return { 474, 310, 402 };
            case 56: return { 196, 1, 53 };
            case 57: return { 197, 1, 54 };
            case 58: return { 198, 1, 55 };
            case 59: return { 256, 0, 311 };
            case 60: return { 457, 0, 27 };
            case 61: return { 458, 0, 27 };
            case 62: return { 0, 0, 0 };
            case 63: return { 127, 1, 1 };
            case 64: return { 203, 4, 65 };
            case 65: return { 0, 0, 0 };
            case 66: return { 335, 159, 209 };
            case 67: return { 1, 22, 2 };
            case 68: return { 0, 0, 0 };
            case 69: return { 329, 51, 20 };
            case 70: return { 337, 52, 6 };
            case 71: return { 338, 52, 6 };
            case 72: return { 323, 0, 0 };
            case 73: return { 167, 0, 177 };
            case 74: return { 173, 0, 38 };
            case 75: return { 162, 0, 0 };
            case 76: return { 278, 0, 400 };
            case 77: return { 204, 4, 0 };
            case 78: return { 445, 214, 246 };
            case 79: return { 321, 0, 1 };
            case 80: return { 324, 0, 0 };
            case 81: return { 322, 0, 1 };
            case 82: return { 325, 0, 0 };
            case 83: return { 200, 43, 62 };
            case 84: return { 329, 13, 70 };
            case 85: return { 330, 13, 70 };
            case 86: return { 161, 28, 3 };
            case 87: return { 352, 87, 90 };
            case 88: return { 354, 87, 92 };
            case 89: return { 354, 1, 93 };
            case 90: return { 355, 1, 93 };
            case 91: return { 408, 10, 98 };
            case 92: return { 210, 154, 116 };
            case 93: return { 480, 114, 117 };
            case 94: return { 481, 114, 117 };
            case 95: return { 0, 0, 0 };
            case 96: return { 0, 0, 0 };
            case 97: return { 211, 1, 118 };
            case 98: return { 129, 115, 1 };
            case 99: return { 212, 0, 119 };
            case 100: return { 330, 5, 120 };
            case 101: return { 331, 5, 120 };
            case 102: return { 134, 132, 2 };
            case 103: return { 130, 0, 1 };
            case 104: return { 357, 86, 95 };
            case 105: return { 359, 86, 125 };
            case 106: return { 360, 86, 125 };
            case 107: return { 364, 0, 129 };
            case 108: return { 362, 85, 127 };
            case 109: return { 131, 3, 486 };
            case 110: return { 365, 88, 130 };
            case 111: return { 406, 0, 454 };
            case 112: return { 407, 0, 454 };
            case 113: return { 13, 18, 1 };
            case 114: return { 342, 121, 132 };
            case 115: return { 343, 121, 132 };
            case 116: return { 0, 0, 0 };
            case 117: return { 164, 1, 133 };
            case 118: return { 446, 217, 249 };
            case 119: return { 215, 5, 134 };
            case 120: return { 226, 203, 173 };
            case 121: return { 0, 0, 0 };
            case 122: return { 0, 0, 0 };
            case 123: return { 184, 1, 25 };
            case 124: return { 218, 2, 138 };
            case 125: return { 476, 10, 0 };
            case 126: return { 477, 10, 0 };
            case 127: return { 460, 5, 178 };
            case 128: return { 0, 0, 0 };
            case 129: return { 439, 133, 179 };
            case 130: return { 167, 0, 133 };
            case 131: return { 333, 12, 204 };
            case 132: return { 461, 155, 205 };
            case 133: return { 485, 2, 213 };
            case 134: return { 461, 4, 221 };
            case 135: return { 486, 2, 213 };
            case 136: return { 462, 4, 221 };
            case 137: return { 336, 162, 212 };
            case 138: return { 337, 165, 210 };
            case 139: return { 138, 47, 1 };
            case 140: return { 339, 11, 226 };
            case 141: return { 136, 1, 0 };
            case 142: return { 450, 1, 234 };
            case 143: return { 451, 1, 234 };
            case 144: return { 243, 1, 238 };
            case 145: return { 442, 209, 240 };
            case 146: return { 167, 2, 232 };
            case 147: return { 248, 5, 256 };
            case 148: return { 255, 280, 304 };
            case 149: return { 389, 86, 306 };
            case 150: return { 390, 86, 307 };
            case 151: return { 390, 86, 306 };
            case 152: return { 391, 86, 307 };
            case 153: return { 132, 287, 4 };
            case 154: return { 244, 1, 239 };
            case 155: return { 259, 1, 314 };
            case 156: return { 257, 1, 313 };
            case 157: return { 260, 289, 315 };
            case 158: return { 173, 0, 38 };
            case 159: return { 133, 295, 4 };
            case 160: return { 484, 218, 344 };
            case 161: return { 485, 218, 344 };
            case 162: return { 396, 106, 350 };
            case 163: return { 394, 87, 349 };
            case 164: return { 395, 87, 349 };
            case 165: return { 322, 0, 1 };
            case 166: return { 0, 0, 0 };
            case 167: return { 0, 0, 0 };
            case 168: return { 471, 1, 373 };
            case 169: return { 472, 1, 376 };
            case 170: return { 472, 1, 373 };
            case 171: return { 473, 1, 376 };
            case 172: return { 254, 240, 276 };
            case 173: return { 402, 106, 158 };
            case 174: return { 266, 0, 377 };
            case 175: return { 274, 5, 389 };
            case 176: return { 269, 1, 370 };
            case 177: return { 322, 0, 1 };
            case 178: return { 241, 0, 236 };
            case 179: return { 0, 0, 0 };
            case 180: return { 273, 1, 203 };
            case 181: return { 489, 311, 403 };
            case 182: return { 295, 319, 407 };
            case 183: return { 331, 10, 413 };
            case 184: return { 474, 6, 19 };
            case 185: return { 491, 4, 424 };
            case 186: return { 288, 0, 425 };
            case 187: return { 490, 311, 403 };
            case 188: return { 296, 319, 407 };
            case 189: return { 332, 10, 413 };
            case 190: return { 475, 6, 19 };
            case 191: return { 492, 4, 424 };
            case 192: return { 289, 0, 425 };
            case 193: return { 290, 0, 433 };
            case 194: return { 475, 323, 436 };
            case 195: return { 325, 0, 512 };
            case 196: return { 493, 20, 437 };
            case 197: return { 476, 323, 436 };
            case 198: return { 326, 0, 512 };
            case 199: return { 494, 20, 437 };
            case 200: return { 447, 324, 438 };
            case 201: return { 0, 0, 0 };
            case 202: return { 0, 0, 0 };
            case 203: return { 299, 2, 443 };
            case 204: return { 300, 2, 443 };
            case 205: return { 323, 0, 0 };
            case 206: return { 162, 3, 154 };
            case 207: return { 453, 17, 395 };
            case 208: return { 304, 1, 461 };
            case 209: return { 305, 1, 461 };
            case 210: return { 323, 0, 0 };
            case 211: return { 0, 385, 474 };
            case 212: return { 478, 5, 12 };
            case 213: return { 479, 5, 12 };
            case 214: return { 282, 320, 416 };
            case 215: return { 501, 4, 481 };
            case 216: return { 308, 350, 482 };
            case 217: return { 309, 3, 484 };
            case 218: return { 502, 4, 481 };
            case 219: return { 309, 350, 482 };
            case 220: return { 310, 3, 484 };
            case 221: return { 195, 0, 494 };
            case 222: return { 511, 358, 495 };
            case 223: return { 0, 0, 0 };
            case 224: return { 287, 320, 502 };
            case 225: return { 287, 363, 502 };
            case 226: return { 0, 0, 0 };
            case 227: return { 492, 1, 503 };
            case 228: return { 493, 1, 503 };
            case 229: return { 0, 0, 0 };
            case 230: return { 0, 0, 0 };
            case 231: return { 453, 4, 505 };
            case 232: return { 454, 4, 505 };
            case 233: return { 0, 0, 507 };
            case 234: return { 511, 359, 511 };
            case 235: return { 317, 5, 515 };
            case 236: return { 318, 5, 515 };
            case 237: return { 0, 0, 255 };
            case 238: return { 323, 0, 198 };
            case 239: return { 2, 22, 1 };
            default: return { 167, 1, 240 }; // Default to Pot
        }
    }

    // Kind 4: Projectiles
    if (kind == 4) {
        switch (id) {
            case 0x01: return { 176, 0, 0 }; // Rock Projectile
            case 0x03: return { 207, 0, 0 }; // Moblin Spear
            case 0x04: return { 208, 0, 0 }; // Deku Seed
            case 0x07: return { 232, 0, 0 }; // Wind Projectile
            case 0x08: return { 233, 0, 0 }; // Fire Projectile
            case 0x09: return { 234, 0, 0 }; // Ice Projectile
            default: return { 176, 0, 0 };
        }
    }

    return { 0, 0, 0 };
}
static inline u32 readU32LE(const u8* p) {
    return static_cast<u32>(p[0]) |
           (static_cast<u32>(p[1]) << 8) |
           (static_cast<u32>(p[2]) << 16) |
           (static_cast<u32>(p[3]) << 24);
}

static inline u16 readU16LE(const u8* p) {
    return static_cast<u16>(p[0]) | (static_cast<u16>(p[1]) << 8);
}

bool EntitySpriteProvider::decodeSpriteRgba(
    const RomDataReader& romReader,
    u16 spriteIndex,
    u16 paletteIndex,
    u16 gfxIndex,
    std::vector<u32>& outPixels,
    int& outWidth,
    int& outHeight,
    u8 frameIndex,
    s8 xOffset,
    s8 yOffset,
    bool clearPixels
) {
    if (!romReader.isLoaded()) return false;

    const u8* rom = romReader.getRomBytes().data();
    const size_t romSize = romReader.getRomSize();

    outWidth = 64;
    outHeight = 64;
    if (clearPixels || outPixels.size() != 64 * 64) {
        outPixels.assign(outWidth * outHeight, 0); // Transparent black
    }

    // 1. Resolve 16-color OBJ Palette
    // 1. Resolve Palette Base Address
    u32 palBaseAddr = 0;
    if (paletteIndex >= 0x16) {
        u32 tableIdx = paletteIndex - 0x16;
        const u32 objPalTableOff = 0x133368;
        if (objPalTableOff + tableIdx * 4 + 4 <= romSize) {
            u32 entry = readU32LE(&rom[objPalTableOff + tableIdx * 4]);
            palBaseAddr = 0x5A2E80 + (entry & 0xFFFFFF);
        }
    } else {
        palBaseAddr = 0x5A2E80 + (static_cast<u32>(paletteIndex) * 32);
    }

    u32 palette[16];
    palette[0] = 0x00000000; // Transparent
    if (palBaseAddr != 0 && palBaseAddr + 32 <= romSize) {
        for (int c = 1; c < 16; ++c) {
            u16 bgr = readU16LE(&rom[palBaseAddr + c * 2]);
            ColorRGBA rgba = RomDataReader::bgr555ToRgba(bgr);
            palette[c] = (static_cast<u32>(rgba.a) << 24) |
                         (static_cast<u32>(rgba.b) << 16) |
                         (static_cast<u32>(rgba.g) << 8)  |
                          static_cast<u32>(rgba.r);
        }
    } else {
        for (int c = 1; c < 16; ++c) palette[c] = 0xFFFFFFFF;
    }

    // 2. Resolve Base Tile Address
    u32 baseTileRomAddr = 0;
    const u32 spriteEntryOffset = 0x0029B4 + (static_cast<u32>(spriteIndex) * 16);
    if (spriteEntryOffset + 16 <= romSize) {
        u32 framesGba = readU32LE(&rom[spriteEntryOffset + 4]);
        u32 ptrGba    = readU32LE(&rom[spriteEntryOffset + 8]);
        if (ptrGba >= 0x08000000u && ptrGba < 0x09000000u) {
            u16 firstTile = 0;
            if (framesGba >= 0x08000000u && framesGba + 4 <= 0x08000000u + romSize) {
                firstTile = readU16LE(&rom[framesGba - 0x08000000u + 2]);
            }
            baseTileRomAddr = (ptrGba - 0x08000000u) + (static_cast<u32>(firstTile) * 32);
        }
    }

    if (baseTileRomAddr == 0 && gfxIndex != 0) {
        u32 fixedGfxOffset = 0x132B30 + (static_cast<u32>(gfxIndex & 0x3FF) * 4);
        if (fixedGfxOffset + 4 <= romSize) {
            u32 fixedVal = readU32LE(&rom[fixedGfxOffset]);
            baseTileRomAddr = 0x5A2E80 + (fixedVal & 0xFFFFFC);
        }
    }

    if (baseTileRomAddr == 0 || baseTileRomAddr >= romSize) {
        // Fallback: try raw 0x5A2E80 offset for basic objects
        baseTileRomAddr = 0x5A2E80 + (static_cast<u32>(gfxIndex & 0x3FF) * 32);
        if (baseTileRomAddr >= romSize) return false;
    }

    // 3. Resolve OAM Pieces from gFrameObjLists (ROM 0x2F3D74)
    const u32 frameObjListsOff = 0x2F3D74;
    u32 pieceCount = 0;
    u32 piecesRomAddr = 0;

    if (frameObjListsOff + (static_cast<u32>(spriteIndex) * 4) + 4 <= romSize) {
        u32 off1 = readU32LE(&rom[frameObjListsOff + spriteIndex * 4]);
        if (frameObjListsOff + off1 + (static_cast<u32>(frameIndex) * 4) + 4 <= romSize) {
            u32 off2 = readU32LE(&rom[frameObjListsOff + off1 + frameIndex * 4]);
            piecesRomAddr = frameObjListsOff + off2;
            if (piecesRomAddr < romSize) {
                pieceCount = rom[piecesRomAddr];
            }
        }
    }

    // GBA Dimensions lookup table: s_dims[shape][size] = { width, height, tilesW, tilesH }
    static const struct { u8 w, h, tw, th; } s_dims[3][4] = {
        /* Square: 8x8, 16x16, 32x32, 64x64 */
        { {8,8,1,1}, {16,16,2,2}, {32,32,4,4}, {64,64,8,8} },
        /* H-Rect: 16x8, 32x8, 32x16, 64x32 */
        { {16,8,2,1}, {32,8,4,1}, {32,16,4,2}, {64,32,8,4} },
        /* V-Rect: 8x16, 8x32, 16x32, 32x64 */
        { {8,16,1,2}, {8,32,1,4}, {16,32,2,4}, {32,64,4,8} }
    };

    int renderedPixels = 0;

    if (pieceCount > 0 && pieceCount < 32 && (piecesRomAddr + 1 + pieceCount * 5 <= romSize)) {
        // Multi-piece rendering centered at (32, 32)
        // Reverse order (pieceCount-1 down to 0) so OAM Entry 0 has highest priority (drawn in front)
        for (int p = static_cast<int>(pieceCount) - 1; p >= 0; --p) {
            const u8* pData = &rom[piecesRomAddr + 1 + p * 5];
            int xoff = static_cast<int>(static_cast<s8>(pData[0]));
            int yoff = static_cast<int>(static_cast<s8>(pData[1]));
            u8 shapeInfo = pData[2];
            u16 tileBase = static_cast<u16>(pData[3] | ((pData[4] & 3) << 8));
            u8 palBank = (pData[4] >> 4) & 0x0F;

            u8 shape = (shapeInfo >> 6) & 3;
            u8 size  = (shapeInfo >> 4) & 3;
            bool hFlip = (shapeInfo & 0x04) != 0;
            bool vFlip = (shapeInfo & 0x08) != 0;
            if (shape >= 3) continue;

            const auto& dim = s_dims[shape][size];
            int startX = 32 + xoff + xOffset;
            int startY = 32 + yoff + yOffset;

            // Load piece-specific palette
            u32 piecePal[16];
            piecePal[0] = 0x00000000;
            u32 piecePalAddr = palBaseAddr + (static_cast<u32>(palBank) * 32);
            if (piecePalAddr + 32 <= romSize) {
                for (int c = 1; c < 16; ++c) {
                    u16 bgr = readU16LE(&rom[piecePalAddr + c * 2]);
                    ColorRGBA rgba = RomDataReader::bgr555ToRgba(bgr);
                    piecePal[c] = (static_cast<u32>(rgba.a) << 24) |
                                  (static_cast<u32>(rgba.b) << 16) |
                                  (static_cast<u32>(rgba.g) << 8)  |
                                   static_cast<u32>(rgba.r);
                }
            } else {
                for (int c = 1; c < 16; ++c) piecePal[c] = palette[c];
            }

            for (int tr = 0; tr < dim.th; ++tr) {
                for (int tc = 0; tc < dim.tw; ++tc) {
                    u32 tIdx = tileBase + (tr * dim.tw) + tc;
                    u32 tileAddr = baseTileRomAddr + (tIdx * 32);
                    if (tileAddr + 32 > romSize) continue;

                    const u8* tileBytes = &rom[tileAddr];
                    for (int py = 0; py < 8; ++py) {
                        for (int px = 0; px < 8; px += 2) {
                            u8 b = tileBytes[py * 4 + px / 2];
                            u8 c0 = b & 0x0F;
                            u8 c1 = (b >> 4) & 0x0F;

                            if (c0 != 0) {
                                int localX = tc * 8 + px;
                                int localY = tr * 8 + py;
                                int finalX = hFlip ? (dim.w - 1 - localX) : localX;
                                int finalY = vFlip ? (dim.h - 1 - localY) : localY;
                                int cx = startX + finalX;
                                int cy = startY + finalY;
                                if (cx >= 0 && cx < 64 && cy >= 0 && cy < 64) {
                                    outPixels[cy * 64 + cx] = piecePal[c0];
                                    renderedPixels++;
                                }
                            }
                            if (c1 != 0) {
                                int localX = tc * 8 + px + 1;
                                int localY = tr * 8 + py;
                                int finalX = hFlip ? (dim.w - 1 - localX) : localX;
                                int finalY = vFlip ? (dim.h - 1 - localY) : localY;
                                int cx = startX + finalX;
                                int cy = startY + finalY;
                                if (cx >= 0 && cx < 64 && cy >= 0 && cy < 64) {
                                    outPixels[cy * 64 + cx] = piecePal[c1];
                                    renderedPixels++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. Fallback: if no OAM pieces were rendered, decode a 16x16 grid at center
    if (renderedPixels == 0) {
        int startX = 24;
        int startY = 24;
        for (int tr = 0; tr < 2; ++tr) {
            for (int tc = 0; tc < 2; ++tc) {
                u32 tIdx = tr * 2 + tc;
                u32 tileAddr = baseTileRomAddr + (tIdx * 32);
                if (tileAddr + 32 > romSize) continue;

                const u8* tileBytes = &rom[tileAddr];
                for (int py = 0; py < 8; ++py) {
                    for (int px = 0; px < 8; px += 2) {
                        u8 b = tileBytes[py * 4 + px / 2];
                        u8 c0 = b & 0x0F;
                        u8 c1 = (b >> 4) & 0x0F;
                        if (c0 != 0) {
                            int cx = startX + (tc * 8) + px;
                            int cy = startY + (tr * 8) + py;
                            if (cx >= 0 && cx < 64 && cy >= 0 && cy < 64) {
                                outPixels[cy * 64 + cx] = palette[c0];
                                renderedPixels++;
                            }
                        }
                        if (c1 != 0) {
                            int cx = startX + (tc * 8) + px + 1;
                            int cy = startY + (tr * 8) + py;
                            if (cx >= 0 && cx < 64 && cy >= 0 && cy < 64) {
                                outPixels[cy * 64 + cx] = palette[c1];
                                renderedPixels++;
                            }
                        }
                    }
                }
            }
        }
    }

    return (renderedPixels > 0);
}

bool EntitySpriteProvider::decodeCompositeSpriteRgba(
    const RomDataReader& romReader,
    const SpriteInfo& info,
    std::vector<u32>& outPixels,
    int& outWidth,
    int& outHeight
) {
    outWidth = 64;
    outHeight = 64;
    outPixels.assign(outWidth * outHeight, 0);

    bool anyOk = false;
    for (u8 i = 0; i < info.partCount; ++i) {
        const auto& part = info.parts[i];
        if (part.spriteIndex == 0) continue;
        int pw = 0, ph = 0;
        bool ok = decodeSpriteRgba(
            romReader,
            part.spriteIndex,
            part.paletteIndex,
            part.gfxIndex,
            outPixels,
            pw, ph,
            part.frameIndex,
            part.xOffset,
            part.yOffset,
            /*clearPixels=*/(i == 0)
        );
        if (ok) anyOk = true;
    }
    return anyOk;
}

SDL_Texture* EntitySpriteProvider::getEntityTexture(
    SDL_Renderer* renderer,
    const RomDataReader& romReader,
    u8 kind,
    u8 id,
    u8 frameIndex
) {
    if (!renderer) return nullptr;

    const u32 cacheKey = (static_cast<u32>(kind) << 16) | (static_cast<u32>(id) << 8) | frameIndex;
    auto it = m_textureCache.find(cacheKey);
    if (it != m_textureCache.end() && it->second != nullptr) {
        return it->second;
    }

    SpriteInfo info = getVanillaSpriteInfo(kind, id);
    if (frameIndex != 0 && info.partCount == 1) {
        info.parts[0].frameIndex = frameIndex;
    }

    std::vector<u32> pixels;
    int w = 64, h = 64;
    bool ok = decodeCompositeSpriteRgba(romReader, info, pixels, w, h);
    if (!ok || pixels.empty()) {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        w, h,
        SDL_PIXELFORMAT_RGBA32,
        pixels.data(),
        w * sizeof(u32)
    );
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        m_textureCache[cacheKey] = texture;
    }

    return texture;
}

} // namespace tmc
