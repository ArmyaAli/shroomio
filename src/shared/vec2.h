#ifndef SHROOM_VEC2_H
#define SHROOM_VEC2_H

typedef struct ShroomVec2 {
  float x;
  float y;
} ShroomVec2;

static inline ShroomVec2 ShroomVec2Sub(ShroomVec2 a, ShroomVec2 b) {
  return (ShroomVec2){a.x - b.x, a.y - b.y};
}

static inline ShroomVec2 ShroomVec2Add(ShroomVec2 a, ShroomVec2 b) {
  return (ShroomVec2){a.x + b.x, a.y + b.y};
}

static inline ShroomVec2 ShroomVec2Scale(ShroomVec2 v, float scale) {
  return (ShroomVec2){v.x * scale, v.y * scale};
}

static inline float ShroomVec2LengthSqr(ShroomVec2 v) { return (v.x * v.x) + (v.y * v.y); }

static inline float ShroomVec2Dot(ShroomVec2 a, ShroomVec2 b) { return (a.x * b.x) + (a.y * b.y); }

#endif
