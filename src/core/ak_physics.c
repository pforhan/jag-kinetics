#include "ak_physics.h"
#include <stddef.h>

// --- Vector Math ---

ak_vec2_t ak_vec2_add(ak_vec2_t a, ak_vec2_t b) {
  return (ak_vec2_t){AK_FIXED_ADD(a.x, b.x), AK_FIXED_ADD(a.y, b.y)};
}

ak_vec2_t ak_vec2_sub(ak_vec2_t a, ak_vec2_t b) {
  return (ak_vec2_t){AK_FIXED_SUB(a.x, b.x), AK_FIXED_SUB(a.y, b.y)};
}

ak_vec2_t ak_vec2_mul(ak_vec2_t v, ak_fixed_t s) {
  return (ak_vec2_t){AK_FIXED_MUL(v.x, s), AK_FIXED_MUL(v.y, s)};
}

ak_fixed_t ak_vec2_dot(ak_vec2_t a, ak_vec2_t b) {
  return AK_FIXED_ADD(AK_FIXED_MUL(a.x, b.x), AK_FIXED_MUL(a.y, b.y));
}

// Safe length squared to prevent overflow
ak_fixed_t ak_vec2_len_sqr(ak_vec2_t v) {
  // A diagonal for 320x240 is 400. 400px = 26,214,400 raw fixed.
  // We use 64-bit to compute the square safely, but the result must fit in
  // 32-bit. x^2 >> 16 < 2^31 => x^2 < 2^47 => x < 11,863,283. Since we sum x^2
  // + y^2, we should limit to ~8M each.
  const ak_fixed_t LIMIT = 8000000;
  if (v.x > LIMIT || v.x < -LIMIT || v.y > LIMIT || v.y < -LIMIT) {
    return 2147483647; // INT32_MAX
  }
  return ak_vec2_dot(v, v);
}

// Safe length using 64-bit intermediates to support screen-width distances
// dist_sqr for >181px overflows 32-bit fixed point.
ak_fixed_t ak_vec2_len(ak_vec2_t v) {
  int64_t x = v.x;
  int64_t y = v.y;
  int64_t sqr = (x * x) + (y * y); // 32.32 format essentially

  // Sqrt logic for 64-bit input to 16.16 output
  // dist = sqrt(x^2 + y^2)
  // We want result in 16.16.
  // Input 'sqr' is effectively (value * 65536)^2.
  // Sqrt(sqr) = value * 65536.
  // So standard integer sqrt of the 64-bit sum gives the correct fixed-point
  // representation directly.

  if (sqr <= 0)
    return 0;

  uint64_t root = 0;
  uint64_t rem = (uint64_t)sqr;
  uint64_t place = 1ULL << 62;

  while (place > rem)
    place >>= 2;

  while (place) {
    if (rem >= root + place) {
      rem -= root + place;
      root += place * 2;
    }
    root >>= 1;
    place >>= 2;
  }

  return (ak_fixed_t)root;
}

// -- World --

void ak_world_init(ak_world_t *world, ak_fixed_t width, ak_fixed_t height,
                   ak_vec2_t gravity) {
  world->width = width;
  world->height = height;
  world->gravity = gravity;
  world->body_count = 0;
  world->tether_count = 0;

  // Scale constants relative to height (standard height 240)
  ak_fixed_t scale_y = AK_FIXED_DIV(height, AK_INT_TO_FIXED(240));
  world->slop = AK_FIXED_MUL(scale_y, AK_INT_TO_FIXED(1) / 100); // 0.01 scaled
  world->max_correction =
      AK_FIXED_MUL(scale_y, AK_INT_TO_FIXED(5)); // 5.0 scaled
}

ak_body_t *ak_world_add_body(ak_world_t *world, ak_shape_t shape, ak_fixed_t x,
                             ak_fixed_t y, ak_fixed_t mass) {
  if (world->body_count >= AK_MAX_BODIES) {
    return 0;
  }
  ak_body_t *b = &world->bodies[world->body_count++];
  b->position = (ak_vec2_t){x, y};
  b->velocity = (ak_vec2_t){0, 0};
  b->force = (ak_vec2_t){0, 0};
  b->shape = shape;
  b->inv_mass = (mass > 0) ? AK_FIXED_DIV(AK_FIXED_ONE, mass) : 0;
  b->restitution = AK_FIXED_DIV(AK_INT_TO_FIXED(7), AK_INT_TO_FIXED(10)); // 0.7
  b->is_static = (mass == 0);
  return b;
}

void ak_world_add_tether(ak_world_t *world, ak_body_t *a, ak_body_t *b,
                         ak_fixed_t max_length) {
  if (world->tether_count >= AK_MAX_TETHERS)
    return;
  ak_tether_t *t = &world->tethers[world->tether_count++];
  t->a = a;
  t->b = b;
  t->max_length_sqr = AK_FIXED_MUL(max_length, max_length);
}

static void ResolveTethers(ak_world_t *world) {
  for (int i = 0; i < world->tether_count; i++) {
    ak_tether_t *t = &world->tethers[i];
    ak_vec2_t diff = ak_vec2_sub(t->b->position, t->a->position);

    // Optimization: Quick AABB rejection first
    ak_fixed_t max_len = AK_FIXED_SQRT(t->max_length_sqr);

    // Quick rejection: if either component > max_len, we are definitely outside
    if (AK_FIXED_ABS(diff.x) <= max_len && AK_FIXED_ABS(diff.y) <= max_len) {
      // Safe to use squared checks if we wanted, but sticking to safe length.
    }

    // Calculate precise safe length (64-bit friendly)
    ak_fixed_t dist = ak_vec2_len(diff);

    if (dist <= max_len)
      continue;

    ak_fixed_t excess = AK_FIXED_SUB(dist, max_len);

    // Normalize diff to get direction: n = diff / dist
    ak_vec2_t n = ak_vec2_mul(diff, AK_FIXED_DIV(AK_FIXED_ONE, dist));

    // SOFT CONSTRAINT & STABILIZATION
    const ak_fixed_t stiffness = AK_INT_TO_FIXED(5) / 10; // 0.5
    ak_fixed_t correction_mag = AK_FIXED_MUL(excess, stiffness);

    // Clamp correction
    ak_fixed_t max_corr = world->max_correction;
    if (correction_mag > max_corr)
      correction_mag = max_corr;

    ak_vec2_t move = ak_vec2_mul(n, correction_mag);

    ak_fixed_t total_imass = AK_FIXED_ADD(t->a->inv_mass, t->b->inv_mass);
    if (total_imass == 0)
      continue;

    if (!t->a->is_static) {
      ak_fixed_t share = AK_FIXED_DIV(t->a->inv_mass, total_imass);
      t->a->position = ak_vec2_add(t->a->position, ak_vec2_mul(move, share));

      ak_fixed_t vrel =
          ak_vec2_dot(ak_vec2_sub(t->b->velocity, t->a->velocity), n);
      if (vrel > 0) {
        // Apply impulse to kill relative velocity
        // P = vrel / total_imass (magnitude of impulse)
        // dV = P * inv_mass * n
        ak_vec2_t P = ak_vec2_mul(n, AK_FIXED_DIV(vrel, total_imass));
        t->a->velocity =
            ak_vec2_add(t->a->velocity, ak_vec2_mul(P, t->a->inv_mass));
      }
    }
    if (!t->b->is_static) {
      ak_fixed_t share = AK_FIXED_DIV(t->b->inv_mass, total_imass);
      t->b->position = ak_vec2_sub(t->b->position, ak_vec2_mul(move, share));

      ak_fixed_t vrel =
          ak_vec2_dot(ak_vec2_sub(t->b->velocity, t->a->velocity), n);
      if (vrel > 0) {
        ak_vec2_t P = ak_vec2_mul(n, AK_FIXED_DIV(vrel, total_imass));
        t->b->velocity =
            ak_vec2_sub(t->b->velocity, ak_vec2_mul(P, t->b->inv_mass));
      }
    }
  }
}

// --- Collision ---

typedef struct {
  ak_body_t *a;
  ak_body_t *b;
  ak_vec2_t normal;
  ak_fixed_t depth;
  int has_collision;
} ak_manifold_t;

ak_manifold_t SolveCircleCircle(ak_body_t *a, ak_body_t *b) {
  ak_manifold_t m = {a, b, {0, 0}, 0, 0};
  ak_vec2_t n = ak_vec2_sub(b->position, a->position);
  ak_fixed_t dist_sqr = ak_vec2_len_sqr(n);
  ak_fixed_t r = AK_FIXED_ADD(a->shape.bounds.circle.radius,
                              b->shape.bounds.circle.radius);

  if (dist_sqr >= AK_FIXED_MUL(r, r))
    return m;
  if (dist_sqr == 0) {
    m.depth = r;
    m.normal = (ak_vec2_t){AK_FIXED_ONE, 0};
    m.has_collision = 1;
    return m;
  }

  ak_fixed_t dist = AK_FIXED_SQRT(dist_sqr);
  m.depth = AK_FIXED_SUB(r, dist);
  m.normal = ak_vec2_mul(n, AK_FIXED_DIV(AK_FIXED_ONE, dist));
  m.has_collision = 1;
  return m;
}

ak_manifold_t SolveAABBAABB(ak_body_t *a, ak_body_t *b) {
  ak_manifold_t m = {a, b, {0, 0}, 0, 0};
  ak_vec2_t n = ak_vec2_sub(b->position, a->position);

  ak_fixed_t a_w = a->shape.bounds.aabb.width;
  ak_fixed_t a_h = a->shape.bounds.aabb.height;
  ak_fixed_t b_w = b->shape.bounds.aabb.width;
  ak_fixed_t b_h = b->shape.bounds.aabb.height;

  ak_fixed_t x_overlap =
      AK_FIXED_SUB(AK_FIXED_ADD(a_w, b_w), AK_FIXED_ABS(n.x));
  if (x_overlap <= 0)
    return m;

  ak_fixed_t y_overlap =
      AK_FIXED_SUB(AK_FIXED_ADD(a_h, b_h), AK_FIXED_ABS(n.y));
  if (y_overlap <= 0)
    return m;

  if (x_overlap < y_overlap) {
    m.depth = x_overlap;
    m.normal = (ak_vec2_t){n.x < 0 ? -AK_FIXED_ONE : AK_FIXED_ONE, 0};
  } else {
    m.depth = y_overlap;
    m.normal = (ak_vec2_t){0, n.y < 0 ? -AK_FIXED_ONE : AK_FIXED_ONE};
  }
  m.has_collision = 1;
  return m;
}

ak_manifold_t SolveCircleAABB(ak_body_t *circle, ak_body_t *aabb) {
  ak_manifold_t m = {circle, aabb, {0, 0}, 0, 0};

  ak_vec2_t diff = ak_vec2_sub(circle->position, aabb->position);
  ak_fixed_t half_w = aabb->shape.bounds.aabb.width;
  ak_fixed_t half_h = aabb->shape.bounds.aabb.height;
  ak_fixed_t clamped_x = AK_FIXED_MAX(-half_w, AK_FIXED_MIN(half_w, diff.x));
  ak_fixed_t clamped_y = AK_FIXED_MAX(-half_h, AK_FIXED_MIN(half_h, diff.y));

  ak_vec2_t closest = {clamped_x, clamped_y};
  ak_vec2_t n = ak_vec2_sub(diff, closest);
  ak_fixed_t dist_sqr = ak_vec2_len_sqr(n);
  ak_fixed_t r = circle->shape.bounds.circle.radius;

  if (dist_sqr > AK_FIXED_MUL(r, r))
    return m;

  m.has_collision = 1;

  if (dist_sqr == 0) {
    if (AK_FIXED_ABS(diff.x) > AK_FIXED_ABS(diff.y)) {
      m.depth = AK_FIXED_ADD(r, half_w);
      // Normal from Circle to AABB (A->B)
      // Diff is Circle - AABB. If diff.x > 0, Circle is to the Right. A->B
      // should be Left (-1).
      m.normal = (ak_vec2_t){diff.x > 0 ? -AK_FIXED_ONE : AK_FIXED_ONE, 0};
    } else {
      m.depth = AK_FIXED_ADD(r, half_h);
      m.normal = (ak_vec2_t){0, diff.y > 0 ? -AK_FIXED_ONE : AK_FIXED_ONE};
    }
    m.depth = r;
  } else {
    ak_fixed_t dist = AK_FIXED_SQRT(dist_sqr);
    m.depth = AK_FIXED_SUB(r, dist);
    // n is Box->Circle. We want A->B (Circle->Box). So negate.
    m.normal = ak_vec2_mul(n, -AK_FIXED_DIV(AK_FIXED_ONE, dist));
  }

  return m;
}

static void ResolveCollision(ak_world_t *world, ak_manifold_t *m) {
  if (!m->has_collision)
    return;

  ak_vec2_t rv = ak_vec2_sub(m->b->velocity, m->a->velocity);
  ak_fixed_t vel_along_normal = ak_vec2_dot(rv, m->normal);

  if (vel_along_normal > 0)
    return;

  ak_fixed_t e = AK_FIXED_MIN(m->a->restitution, m->b->restitution);
  ak_fixed_t j = AK_FIXED_MUL(-(AK_FIXED_ONE + e), vel_along_normal);
  ak_fixed_t den = AK_FIXED_ADD(m->a->inv_mass, m->b->inv_mass);

  if (den == 0)
    return;

  j = AK_FIXED_DIV(j, den);

  ak_vec2_t impulse = ak_vec2_mul(m->normal, j);

  if (!m->a->is_static)
    m->a->velocity =
        ak_vec2_sub(m->a->velocity, ak_vec2_mul(impulse, m->a->inv_mass));
  if (!m->b->is_static)
    m->b->velocity =
        ak_vec2_add(m->b->velocity, ak_vec2_mul(impulse, m->b->inv_mass));

  const ak_fixed_t percent = AK_INT_TO_FIXED(2) / 10; // 0.2
  const ak_fixed_t slop = world->slop;

  ak_fixed_t correction_mag = AK_FIXED_MAX(AK_FIXED_SUB(m->depth, slop), 0);
  ak_fixed_t corr_num = AK_FIXED_MUL(correction_mag, percent);
  correction_mag = AK_FIXED_DIV(corr_num, den);
  ak_vec2_t correction = ak_vec2_mul(m->normal, correction_mag);

  if (!m->a->is_static)
    m->a->position =
        ak_vec2_sub(m->a->position, ak_vec2_mul(correction, m->a->inv_mass));
  if (!m->b->is_static)
    m->b->position =
        ak_vec2_add(m->b->position, ak_vec2_mul(correction, m->b->inv_mass));
}

void ak_world_step(ak_world_t *world, ak_fixed_t dt) {
  for (int i = 0; i < world->body_count; i++) {
    ak_body_t *b = &world->bodies[i];
    if (b->is_static)
      continue;

    // Apply gravity
    b->force = ak_vec2_add(
        b->force,
        ak_vec2_mul(world->gravity, AK_FIXED_DIV(AK_FIXED_ONE, b->inv_mass)));

    // Integrate Velocity
    ak_vec2_t acceleration = ak_vec2_mul(b->force, b->inv_mass);
    b->velocity = ak_vec2_add(b->velocity, ak_vec2_mul(acceleration, dt));

    // Integrate Position
    b->position = ak_vec2_add(b->position, ak_vec2_mul(b->velocity, dt));

    // Reset force
    b->force = (ak_vec2_t){0, 0};
  }

  // Collisions
  for (int i = 0; i < world->body_count; i++) {
    for (int j = i + 1; j < world->body_count; j++) {
      ak_manifold_t m = {0};
      ak_body_t *a = &world->bodies[i];
      ak_body_t *b = &world->bodies[j];

      if (a->is_static && b->is_static)
        continue;

      if (a->shape.type == AK_SHAPE_CIRCLE &&
          b->shape.type == AK_SHAPE_CIRCLE) {
        m = SolveCircleCircle(a, b);
      } else if (a->shape.type == AK_SHAPE_AABB &&
                 b->shape.type == AK_SHAPE_AABB) {
        m = SolveAABBAABB(a, b);
      } else if (a->shape.type == AK_SHAPE_CIRCLE &&
                 b->shape.type == AK_SHAPE_AABB) {
        m = SolveCircleAABB(a, b);
      } else if (a->shape.type == AK_SHAPE_AABB &&
                 b->shape.type == AK_SHAPE_CIRCLE) {
        m = SolveCircleAABB(b, a);
        m.normal = ak_vec2_mul(m.normal, -AK_FIXED_ONE);
        m.a = a;
        m.b = b;
      }

      if (m.has_collision) {
        ResolveCollision(world, &m);
      }
    }
  }

  // Tethers
  ResolveTethers(world);
}
