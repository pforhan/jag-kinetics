#include "ak_demo_setup.h"
#include "ak_physics.h"
#include "pd_api.h"


static ak_world_t world;
static PlaydateAPI *pd = NULL;

static int update(void *userdata) {
  pd->graphics->clear(kColorWhite);

  ak_fixed_t dt = AK_INT_TO_FIXED(1) / 30; // 30 FPS
  ak_world_step(&world, dt);

  for (int i = 0; i < world.body_count; i++) {
    ak_body_t *b = &world.bodies[i];
    int x = AK_FIXED_TO_INT(b->position.x);
    int y = AK_FIXED_TO_INT(b->position.y);

    if (b->shape.type == AK_SHAPE_CIRCLE) {
      int r = AK_FIXED_TO_INT(b->shape.bounds.circle.radius);
      pd->graphics->drawEllipse(x - r, y - r, r * 2, r * 2, 1, 0, 360,
                                kColorBlack);
    } else if (b->shape.type == AK_SHAPE_AABB) {
      int w = AK_FIXED_TO_INT(b->shape.bounds.aabb.width);
      int h = AK_FIXED_TO_INT(b->shape.bounds.aabb.height);
      pd->graphics->drawRect(x - w, y - h, w * 2, h * 2, kColorBlack);
    }
  }

  return 1;
}

int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, uint32_t arg) {
  if (event == kEventInit) {
    pd = playdate;
    ak_demo_create_standard_scene(&world);
    pd->system->setUpdateCallback(update, NULL);
  }
  return 0;
}
