#include "ak_demo_setup.h"

void ak_demo_create_standard_scene(ak_world_t *world) {
  ak_world_init(world, (ak_vec2_t){0, AK_INT_TO_FIXED(50)}); // Gravity down

  // 1. Ground (Static AABB)
  ak_world_add_body(
      world,
      (ak_shape_t){.type = AK_SHAPE_AABB,
                   .bounds.aabb = {AK_INT_TO_FIXED(160), AK_INT_TO_FIXED(10)}},
      AK_INT_TO_FIXED(160), AK_INT_TO_FIXED(230), 0);

  // 2. Anchored Pendulum (Center)
  ak_body_t *anchor =
      ak_world_add_body(world,
                        (ak_shape_t){.type = AK_SHAPE_CIRCLE,
                                     .bounds.circle = {AK_INT_TO_FIXED(2)}},
                        AK_INT_TO_FIXED(160), AK_INT_TO_FIXED(40), 0); // Static

  ak_body_t *bob = ak_world_add_body(
      world,
      (ak_shape_t){.type = AK_SHAPE_CIRCLE,
                   .bounds.circle = {AK_INT_TO_FIXED(10)}},
      AK_INT_TO_FIXED(220), AK_INT_TO_FIXED(40), AK_INT_TO_FIXED(5)); // Mass 5

  ak_world_add_tether(world, anchor, bob, AK_INT_TO_FIXED(60));

  // 3. Free Falling Box (Left)
  ak_world_add_body(
      world,
      (ak_shape_t){.type = AK_SHAPE_AABB,
                   .bounds.aabb = {AK_INT_TO_FIXED(10), AK_INT_TO_FIXED(10)}},
      AK_INT_TO_FIXED(60), AK_INT_TO_FIXED(50), AK_INT_TO_FIXED(2));

  // 4. Free Falling Circle (Right)
  ak_world_add_body(world,
                    (ak_shape_t){.type = AK_SHAPE_CIRCLE,
                                 .bounds.circle = {AK_INT_TO_FIXED(12)}},
                    AK_INT_TO_FIXED(260), AK_INT_TO_FIXED(30),
                    AK_INT_TO_FIXED(2));

  // 5. Tethered trio (Bolas)
  ak_body_t *b1 = ak_world_add_body(
      world,
      (ak_shape_t){.type = AK_SHAPE_CIRCLE,
                   .bounds.circle = {AK_INT_TO_FIXED(8)}},
      AK_INT_TO_FIXED(100), AK_INT_TO_FIXED(80), AK_INT_TO_FIXED(3));

  ak_body_t *b2 = ak_world_add_body(
      world,
      (ak_shape_t){.type = AK_SHAPE_CIRCLE,
                   .bounds.circle = {AK_INT_TO_FIXED(8)}},
      AK_INT_TO_FIXED(130), AK_INT_TO_FIXED(80), AK_INT_TO_FIXED(3));

  ak_body_t *b3 = ak_world_add_body(
      world,
      (ak_shape_t){.type = AK_SHAPE_CIRCLE,
                   .bounds.circle = {AK_INT_TO_FIXED(6)}},
      AK_INT_TO_FIXED(160), AK_INT_TO_FIXED(60), AK_INT_TO_FIXED(2));

  b2->velocity.x = AK_INT_TO_FIXED(20);

  ak_world_add_tether(world, b1, b2, AK_INT_TO_FIXED(40));
  ak_world_add_tether(world, b2, b3, AK_INT_TO_FIXED(40));
}
