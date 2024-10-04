#include <raylib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "arr.h"
#include "err.h"
#include <tgmath.h>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOGDI
#define _WINUSER_
#include <libwebsockets.h>

#define pk(buf, type, off) *(type *)(buf + off)

typedef uint8_t u8;

/*--- state ---*/

float w, h;

typedef enum activity : uint32_t
{
  ACT_intro,
  ACT_conway_set,
  ACT_conway_run,
  ACT_N
} activity;

activity act;

typedef struct plyr
{
  int id, team, has_team;
  float y, ry;
  u8 *pack;
  bool active;
} plyr;

plyr *plyrs;
pthread_mutex_t plyr_mtx;
int *empty;

uint64_t *cg_0, *cg_1;
pthread_mutex_t cg_mtx;
const uint64_t cg_w = 256, cg_h = 256;

bool 
cg_get(uint64_t *buf, uint64_t x, uint64_t y)
{
  if (x < 0 || y < 0 || x >= cg_w || y >= cg_h) return false;
  return (buf[y * (cg_w / 64) + x / 64] >> (x % 64)) & 1;
}

void 
cg_set(uint64_t *buf, uint64_t x, uint64_t y, bool val)
{
  if (x < 0 || y < 0 || x >= cg_w || y >= cg_h) return;
  (buf[y * (cg_w / 64) + x / 64]) &= ~(1ULL << (x % 64));
  (buf[y * (cg_w / 64) + x / 64]) |= (((uint64_t)val) << (x % 64));
}

void
cg_step()
{
  pthread_mutex_lock(&cg_mtx);

  for (int j = 0; j < cg_h; j++)
  for (int i = 0; i < cg_w; i++) {
    int k = 0;
    k += cg_get(cg_0, i - 1, j - 1);
    k += cg_get(cg_0, i - 1, j);
    k += cg_get(cg_0, i - 1, j + 1);
    k += cg_get(cg_0, i, j - 1);
    k += cg_get(cg_0, i, j + 1);
    k += cg_get(cg_0, i + 1, j - 1);
    k += cg_get(cg_0, i + 1, j);
    k += cg_get(cg_0, i + 1, j + 1);
    bool l = cg_get(cg_0, i, j);
    if (l) {
      if (k <= 1) cg_set(cg_1, i, j, false);
      else if (k == 2 || k == 3) cg_set(cg_1, i, j, true);
      else if (k >= 4) cg_set(cg_1, i, j, false);
    } else {
      if (k == 3) cg_set(cg_1, i, j, true);
      else cg_set(cg_1, i, j, false);
    }
  }

  auto t = cg_0;
  cg_0 = cg_1;
  cg_1 = t;

  pthread_mutex_unlock(&cg_mtx);
}

void
st_init()
{
  plyrs = arr_new(plyr);
  empty = arr_new(int);
  
  size_t buf_size = cg_w * cg_h;
  cg_0 = malloc(buf_size);
  memset(cg_0, 0, buf_size);
  cg_1 = malloc(buf_size);
  memset(cg_1, 0, buf_size);

  pthread_mutex_init(&cg_mtx, NULL);
  pthread_mutex_init(&plyr_mtx, NULL);
}

/*--- networking ---*/

char pre_buf[LWS_PRE];

typedef struct lws lws;
typedef enum lws_callback_reasons lws_reason; 
typedef struct lws_protocols lws_protocol;
typedef struct lws_context_creation_info lws_ctx_inf;
typedef struct lws_context lws_ctx;

typedef struct session
{
  int id;
  time_t prev_time;
} session;

lws_ctx *ws_ctx;

typedef enum payload_type : u8
{
  PT_pos,
  PT_new_player,
  PT_del_player,
  PT_ack_conn,
  PT_activity,
  PT_conway_set,
  PT_conway_clear,
  PT_team_select,
  PT_importance,
} payload_type;

bool 
only_not(plyr *p, void *user)
{
  return p->id != *(int *)user;
}

bool
only_eq(plyr *p, void *user)
{
  return p->id == *(int *)user;
}

bool
only_all(plyr *p, void *user)
{
  return true;
}

void
pack_queue(void *data, size_t len, void *user, bool (*filter)(plyr *, void *));

void
pack_recv_intro(payload_type pt, session *ss, void *in, size_t len)
{
  switch (pt) {
    case PT_pos: {
      if (len < 3) {
        return;
      }

      auto y = *(uint16_t *)(in + 1);

      pthread_mutex_lock(&plyr_mtx);
      plyrs[ss->id].y = (float)y / (float)UINT16_MAX;
      pthread_mutex_unlock(&plyr_mtx);
    } break;
    case PT_team_select: {
      if (len < 2) {
        return;
      }

      u8 team = *(u8 *)(in + 1);
      pthread_mutex_lock(&plyr_mtx);
      plyrs[ss->id].team = team;
      plyrs[ss->id].has_team = true;
      pthread_mutex_unlock(&plyr_mtx);
    } break;
  }
}

void
pack_recv_conway(payload_type pt, session *ss, void *in, size_t len)
{
  switch (pt) {
    case PT_conway_set: {
      if (len < 6) {
        return;
      }

      pthread_mutex_lock(&cg_mtx);
      u8 buf[6];
      auto pos = (uint16_t *)(in + 1);
      auto yes = *(u8 *)(in + 5);
      cg_set(cg_0, pos[0], pos[1], yes);

      memcpy(buf, in, sizeof(buf));
      pack_queue(buf, len, &(int){ss->id}, only_not);
      pthread_mutex_unlock(&cg_mtx);
    } break;
  }
}

void 
pack_recv(lws *wsi, session *ss, void *in, size_t len) 
{
  auto pt = *(payload_type *)in;
  bool final = lws_is_final_fragment(wsi);
  switch (act) {
    case ACT_intro: return pack_recv_intro(pt, ss, in, len);
    case ACT_conway_set: return pack_recv_conway(pt, ss, in, len);
  }
}

void
pack_queue(void *data, size_t len, void *user, bool (*filter)(plyr *, void *))
{
  pthread_mutex_lock(&plyr_mtx);
  u8 *d = data;
  for (int i = 0; i < arr_len(plyrs); i++) {
    plyr *p = plyrs + i;
    if (!filter(p, user)) continue;
    arr_add_arr(&p->pack, data, len, sizeof(u8));
  }
  pthread_mutex_unlock(&plyr_mtx);
}

int
ws_callback(lws *wsi, lws_reason reason, void *user, void *in, size_t len)
{
  auto ss = (session *)user;

  switch (reason) {
    case LWS_CALLBACK_ESTABLISHED: {
      pthread_mutex_lock(&plyr_mtx);
      int id;
      if (arr_len(empty) != 0) {
        arr_pop(&empty, &id);
        ss->id = id;
        plyrs[id].id = id;
        plyrs[id].active = true;
        plyrs[id].has_team = false;
        arr_clear(plyrs[id].pack);
        arr_add_arr(&plyrs[id].pack, pre_buf, LWS_PRE, sizeof(u8));
      } else {
        id = ss->id = arr_len(plyrs);
        arr_add(&plyrs, &(plyr){.active=true, .pack=arr_new(u8), .id=id});
        arr_add_arr(&plyrs[id].pack, pre_buf, LWS_PRE, sizeof(u8));
      }
      pthread_mutex_unlock(&plyr_mtx);

      u8 buf[7];
      pk(buf, uint8_t, 0) = PT_new_player;
      pk(buf, int, 1) = id;
      pk(buf, uint16_t, 4) = 0;
      pack_queue(buf, sizeof(buf), &(int){id}, only_not);

      pk(buf, uint8_t, 0) = PT_ack_conn;
      pk(buf, int, 1) = id;
      pk(buf, uint8_t, 5) = act;
      pack_queue(buf, 6, &(int){id}, only_eq);
      lws_callback_on_writable_all_protocol(ws_ctx, lws_get_protocol(wsi));

      if (act == ACT_conway_set) {
        pthread_mutex_lock(&cg_mtx);
        u8 buf[6];
        for (int j = 0; j < 256; j++)
        for (int i = 0; i < 256; i++) {
          if (cg_get(cg_0, i, j)) {
            pk(buf, uint8_t, 0) = PT_conway_set;
            pk(buf, uint16_t, 1) = i;
            pk(buf, uint16_t, 3) = j;
            pk(buf, uint8_t, 5) = true;
            pack_queue(buf, sizeof(buf), &(int){id}, only_eq);
          }
        }

        pthread_mutex_unlock(&cg_mtx);
      }
    } break;
    case LWS_CALLBACK_SERVER_WRITEABLE: {
      u8 *pack = plyrs[ss->id].pack;
      
      if (arr_len(pack) - LWS_PRE != 0) {
        lws_write(wsi, pack + LWS_PRE, arr_len(pack) - LWS_PRE, LWS_WRITE_BINARY);
        arr_setlen(pack, LWS_PRE);
      }

      lws_callback_on_writable_all_protocol(ws_ctx, lws_get_protocol(wsi));
    } break;
    case LWS_CALLBACK_RECEIVE: {
      pack_recv(wsi, ss, in, len);
      lws_callback_on_writable_all_protocol(ws_ctx, lws_get_protocol(wsi));
    } break;
    case LWS_CALLBACK_CLOSED: {
      plyrs[ss->id].active = plyrs[ss->id].has_team = false;
      arr_add(&empty, &ss->id);
    } break;
  }

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

void*
ws_thread(void *user)
{
  while (!WindowShouldClose()) {
    lws_service(ws_ctx, 0);
  }

  return NULL;
}

void
ws_init()
{
  srand(time(NULL));

  static lws_protocol protocols[] = {
    {"test", ws_callback, sizeof(session), 0, 0, NULL, 0},
    {}
  };

  lws_ctx_inf inf = {
    .port = 8080,
    .protocols = protocols
  };

  ws_ctx = lws_create_context(&inf);

  if (!ws_ctx) {
    fprintf(stderr, "failed to create ws context\n");
    exit(-1);
  }

  pthread_t thread;
  pthread_create(&thread, NULL, &ws_thread, NULL);
}

/*--- rendering ---*/

struct {
  float x, y;
  float vx, vy;
  float prev_avg_y_0, prev_avg_y_1;
  float start_time;
  float lu_time;
  int l_score, r_score;
  int side;
} pong;

bool
filter_same_team(plyr *p, void *user) {
  return p->team == *(int *)user;
}

void
draw_intro()
{
  BeginBlendMode(2);
  DrawRectangleV((Vector2){0, 0}, (Vector2){GetScreenWidth(), GetScreenHeight()}, (Color){0xf0, 0xf0, 0xf0, 0xff});
  EndBlendMode();

  bool is_started = pong.start_time < GetTime();

  if (is_started) {
    const int font_size = 100;
    const int pad = 20;
    char buf[10];
    sprintf_s(buf, sizeof(buf), "%d", pong.l_score);
    auto size = MeasureTextEx(GetFontDefault(), buf, font_size, 1);
    DrawText(buf, w / 2 - size.x - pad, pad, font_size, WHITE);

    sprintf_s(buf, sizeof(buf), "%d", pong.r_score);
    DrawText(buf, w / 2 + pad, pad, font_size, WHITE);
  }

  if (is_started && GetTime() - pong.lu_time > 0.01667) {
    pong.x += pong.vx;
    pong.y += pong.vy;
    pong.lu_time = GetTime();
  }

  BeginBlendMode(1);

  float avg_y[] = {0, 0};
  float n_y[] = {0, 0};

  pthread_mutex_lock(&plyr_mtx);
  for (plyr *p = plyrs, *e = arr_end(p); p != e; p++) {
    if (!p->active) continue;
    p->ry = p->ry + (p->y - p->ry) * 0.01;
    avg_y[p->team] += p->ry * h;
    n_y[p->team]++;

    DrawCircleV((Vector2){w / 2 + (p->team * 2 - 1) * (w / 2 - 20), p->ry * h}, 10, p->team ? (Color){0xff, 0x1d, 0xcf, 100} : (Color){0xff, 0xd3, 0x02, 100});
  }
  pthread_mutex_unlock(&plyr_mtx);

  DrawCircleV((Vector2){pong.x, pong.y}, 15, WHITE);

  if (n_y[0] > 0) avg_y[0] /= n_y[0];
  if (n_y[1] > 0) avg_y[1] /= n_y[1];

  DrawRectangleV((Vector2){40, avg_y[0] - 80}, (Vector2){10, 160}, (Color){0xff, 0xd3, 0x02, 100});
  DrawRectangleV((Vector2){w - 50, avg_y[1] - 80}, (Vector2){10, 160}, (Color){0xff, 0x1d, 0xcf, 100});

  if (pong.y > h - 15) {
    pong.y = h - 15;
    pong.vy *= -1;
  }

  if (pong.y < 15) {
    pong.y = 15;
    pong.vy *= -1;
  }

  int won = false;

  if (pong.x < 0) {
    pong.r_score++;
    won = 1;
  }

  if (pong.x > w) {
    pong.l_score++;
    won = -1;
  }

  float len = hypot(pong.vx, pong.vy);
  bool hit = false;
  if (pong.x < 65 && pong.x > 55 && fabs(pong.y - avg_y[0]) < 80) {
    pong.x = 65;
    pong.vx *= -1;
    pong.vy += (avg_y[0] - pong.prev_avg_y_0) * 2;
    hit = true;
  }

  if (pong.x > w - 65 && pong.x < w - 55 && fabs(pong.y - avg_y[1]) < 80) {
    pong.x = w - 65;
    pong.vx *= -1;
    pong.vy += (avg_y[1] - pong.prev_avg_y_1) * 2;
    hit = true;
  }

  if (hit) {
    float new_len = hypot(pong.vx, pong.vy);
    pong.vx /= new_len;
    pong.vy /= new_len;
    pong.vx *= len * 1.3;
    pong.vy *= len * 1.3;
  }

  DrawRectangleV((Vector2){w / 2 - 3, 0}, (Vector2){6, h}, (Color){0xff, 0xff, 0xff, 100}); 

  EndBlendMode();

  if (IsKeyPressed(KEY_S) || pong.start_time == 0 || won)  {
    pong.start_time = GetTime() + 3.1;
    pong.x = w / 2;
    pong.y = h / 2;
    pong.vx = (won ? won : -1) * 8;
    pong.vy = 0;
    if (IsKeyPressed(KEY_S)) {
      pong.l_score = pong.r_score = 0;
    }

    fprintf(stderr, "pong.start_time: %f", pong.start_time);
  }

  if (GetTime() < pong.start_time) {
    char buf[2];
    sprintf_s(buf, sizeof(buf), "%d", (int)-floor(GetTime() - pong.start_time));
    const int font_size = 100;
    auto size = MeasureTextEx(GetFontDefault(), buf, font_size, 1);
    DrawRectangleV((Vector2){w / 2 - size.x / 2 - 10, h / 2 - size.y / 2 - 10}, (Vector2){size.x + 20, size.y + 20}, BLACK);
    DrawText(buf, w / 2 - size.x / 2, h / 2 - size.y / 2, font_size, WHITE);
  }

  pong.prev_avg_y_0 = avg_y[0];
  pong.prev_avg_y_1 = avg_y[1];

  int side = (pong.vx < 0) ? 0 : 1;
  if (side != pong.side) {
    u8 buf_impt[2];
    buf_impt[0] = PT_importance;
    buf_impt[1] = 1;
    u8 buf_nimpt[2];
    buf_nimpt[0] = PT_importance;
    buf_nimpt[1] = 0;

    pack_queue(buf_impt, sizeof(buf_impt), &(int){side}, filter_same_team);
    pack_queue(buf_nimpt, sizeof(buf_nimpt), &(int){!side}, filter_same_team);

    pong.side = side;
  }
}

void
draw_cg()
{
  if (act == ACT_conway_run) {
    cg_step();
  }

  auto sw = (float)w/(float)cg_w;
  auto sh = (float)h/(float)cg_h;

  if (act == ACT_conway_set) {
    Color color = BLACK;
    if (IsKeyDown(KEY_LEFT_CONTROL)) {
      color = LIGHTGRAY;
    }

    auto mp = GetMousePosition();
    DrawRectangleV((Vector2){floor(mp.x / sw) * sw, floor(mp.y / sh) * sh}, (Vector2){sw, sh}, color);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      cg_set(cg_0, mp.x / sw, mp.y / sh, !cg_get(cg_0, mp.x / sw, mp.y / sh));

      u8 buf[9];
      *(int *)(buf) = PT_conway_set;
      *(uint16_t *)(buf + 4) = mp.x / sw;
      *(uint16_t *)(buf + 6) = mp.y / sh;
      *(u8 *)(buf + 8) = cg_get(cg_0, mp.x / sw, mp.y / sh);

      pack_queue(buf, sizeof(buf), NULL, only_all);
    }
  }

  ClearBackground(BLACK);

  for (int j = 0; j < cg_h; j++)
  for (int i = 0; i < cg_w; i++) {
    if (!cg_get(cg_0, i, j)) continue;
    DrawRectangleV((Vector2){i * sw, j * sh}, (Vector2){sw, sh}, WHITE);
  }
}

void 
draw() 
{
  draw_intro();
}

/*--- main ---*/

int 
main(int argc, char **argv) 
{
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(2304, 1440, "test");
  
  st_init();
  ws_init();
  while (!WindowShouldClose()) {
    if (IsWindowFullscreen()) {
//      w = GetMonitorWidth();
//      h = GetMonitorHeight();
    } else {
      w = GetScreenWidth();
      h = GetScreenHeight();
    }
    BeginDrawing();
    draw();
    EndDrawing();
    if (IsKeyPressed(KEY_F11)) {
      ToggleBorderlessWindowed();
    }
  }

  CloseWindow();
  lws_context_destroy(ws_ctx);
}
