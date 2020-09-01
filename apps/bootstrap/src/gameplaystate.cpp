#include "gameplaystate.h"

#include "assets.h"
#include "bootstrap.h"

namespace Bootstrap
{

bool GamePlayState::on_init(App* app)
{
    _app = app;

    if (!_tilemap.load(GliAssetPath("maps/main.bin")))
    {
        return false;
    }

    if (!_player.load(GliAssetPath("sprites/droid.png")))
    {
        return false;
    }

    if (!_foe.load(GliAssetPath("sprites/illegal_opcode.png")))
    {
        return false;
    }

    if (!_status_panel.load(GliAssetPath("gui/status_panel.png")))
    {
        return false;
    }

    if (!_leds.load(GliAssetPath("gui/leds.png")))
    {
        return false;
    }

    if (!_nmi_dark.load(GliAssetPath("fx/nmi_dark.png")))
    {
        return false;
    }

    if (!_nmi_shock.load(GliAssetPath("fx/nmi_shock.png")))
    {
        return false;
    }

    return true;
}


void GamePlayState::on_destroy() {}


bool GamePlayState::on_enter()
{
    _dbus = 0xA5;
    _abus_hi = 0xDE;
    _abus_lo = 0xAD;
    _a_reg = 0xBE;
    _x_reg = 0x55;
    _y_reg = 0xAA;
    _nmitimer = 0.0f;
    _nmifired = 0.0f;

    _movables.resize(2);
    _movables[0].sprite = &_player;
    _movables[0].position = { 9.5f * _tilemap.tile_size(), 8.5f * _tilemap.tile_size() };
    _movables[0].velocity = { 0.0f, 0.0f };
    _movables[0].radius = 11.0f;
    _movables[0].frame = 1;

    _movables[1].sprite = &_foe;
    _movables[1].position = { 9.5f * _tilemap.tile_size(), 28.5f * _tilemap.tile_size() };
    _movables[1].velocity = { 0.0f, 0.0f };
    _movables[1].radius = 11.0f;
    _movables[1].frame = 1;

    return true;
}


void GamePlayState::on_exit()
{
    _movables.clear();
}


void GamePlayState::on_suspend() {}


void GamePlayState::on_resume() {}


static const float NmiDarkInEnd = 0.15f;
static const float NmiZapInEnd = 0.2f;
static const float NmiZapOutStart = 0.4f;
static const float NmiZapOutEnd = 0.6f;
static const float NmiDarkOutEnd = 1.0f;
static const float NmiCooldown = 1.2f;
static const float NmiDuration = NmiCooldown;


static float clamp(float t, float min, float max)
{
    return t < min ? min : (t > max ? max : t);
}


static float ease_in(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - t, 3.0f);
}


static float ease_out(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(t, 3.0f);
}


bool GamePlayState::on_update(float delta)
{
    _app->clear_screen(gli::Pixel(0x1F1D2C));

    // Pixel units..
    static const float min_v = 16.0f;
    static const float drag = 0.95f;
    static const float dv = 192.0f;

    for (Movable& movable : _movables)
    {
        movable.velocity.x = std::abs(movable.velocity.x) < min_v ? 0.0f : movable.velocity.x * drag;
        movable.velocity.y = std::abs(movable.velocity.y) < min_v ? 0.0f : movable.velocity.y * drag;
    }

    Movable& player = _movables[0];

    if (_app->key_state(gli::Key_W).down)
    {
        player.velocity.y = -dv;
    }

    if (_app->key_state(gli::Key_S).down)
    {
        player.velocity.y = dv;
    }

    if (_app->key_state(gli::Key_A).down)
    {
        player.velocity.x = -dv;
    }

    if (_app->key_state(gli::Key_D).down)
    {
        player.velocity.x = dv;
    }

    if (player.velocity.x < 0.0f)
    {
        player.frame = 0;
    }
    else if (player.velocity.x > 0.0f)
    {
        player.frame = 1;
    }

    // Clip movement
    float move_x = player.velocity.x * delta;
    float move_y = player.velocity.y * delta;

    if (check_collision(player.position.x + move_x, player.position.y + move_y, player.radius))
    {
        if (!check_collision(player.position.x + move_x, player.position.y, player.radius))
        {
            move_y = 0.0f;
            player.velocity.y = 0.0f;
        }
        else if (!check_collision(player.position.x, player.position.y + move_y, player.radius))
        {
            move_x = 0.0f;
            player.velocity.x = 0.0f;
        }
        else
        {
            move_x = 0.0f;
            move_y = 0.0f;
            player.velocity.x = 0.0f;
            player.velocity.y = 0.0f;
        }
    }

    player.position.x += move_x;
    player.position.y += move_y;

    _movables[1].frame = player.position.x < _movables[1].position.x ? 0 : 1;

    if (_app->key_state(gli::Key_Space).pressed)
    {
        _nmifired = 0.5f;
    }
    else if (_nmifired >= delta)
    {
        _nmifired -= delta;
    }
    else
    {
        _nmifired = 0.0f;
    }

    if (_nmitimer >= delta)
    {
        _nmitimer -= delta;
    }
    else
    {
        _nmitimer = 0.0f;

        if (_nmifired > 0.0f)
        {
            _nmitimer = NmiDuration;
        }
    }

    // playfield = 412 x 360
    _cx = (int)(player.position.x + 0.5f) - 206;
    _cy = (int)(player.position.y + 0.5f) - 180;

    int coarse_scroll_x = _cx / _tilemap.tile_size();
    int coarse_scroll_y = _cy / _tilemap.tile_size();
    int fine_scroll_x = _cx % _tilemap.tile_size();
    int fine_scroll_y = _cy % _tilemap.tile_size();

    int sy = -fine_scroll_y;

    for (int y = coarse_scroll_y; y < _tilemap.height(); ++y)
    {
        int sx = -fine_scroll_x;

        for (int x = coarse_scroll_x; x < _tilemap.width(); ++x)
        {
            int t = _tilemap(x, y);

            if (t)
            {
                int ox;
                int oy;
                bool has_alpha;
                _tilemap.draw_info(t - 1, ox, oy, has_alpha);

                if (has_alpha)
                {
                    _app->blend_partial_sprite(sx, sy, _tilemap.tilesheet(),  ox, oy, _tilemap.tile_size(), _tilemap.tile_size(), 255);
                }
                else
                {
                    _app->draw_partial_sprite(sx, sy, &_tilemap.tilesheet(), ox, oy, _tilemap.tile_size(), _tilemap.tile_size());
                }
            }

            sx += _tilemap.tile_size();

            if (sx >= _app->screen_width())
            {
                break;
            }
        }

        sy += _tilemap.tile_size();

        if (sy >= _app->screen_height())
        {
            break;
        }
    }

    // fx - pre-sprites
    if (_nmitimer)
    {
        draw_nmi(player.position);

    }

    // sprites
    for (Movable& movable : _movables)
    {
        draw_sprite(movable.position.x, movable.position.y, *movable.sprite, movable.frame);
    }

    // fx - post-sprites

    // GUI
    _a_reg = _cx & 0xFF;
    _x_reg = _cy & 0xFF;

    _app->draw_sprite(412, 4, &_status_panel);
    draw_register(_dbus, 455, 48, 0);
    draw_register(_abus_lo, 455, 101, 2);
    draw_register(_abus_hi, 455, 120, 2);
    draw_register(_a_reg, 455, 167, 1);
    draw_register(_x_reg, 455, 214, 1);
    draw_register(_y_reg, 455, 261, 1);

    return true;
}


void GamePlayState::draw_register(uint8_t reg, int x, int y, int color)
{
    int ox = 0;
    int oy = color * 17;

    for (int i = 8; i--;)
    {
        uint8_t bit = (reg >> i) & 1;
        ox = bit * 17;
        _app->blend_partial_sprite(x, y, _leds, ox, oy, 17, 17, 255);
        x += 18;
    }
}


void GamePlayState::draw_sprite(float x, float y, gli::Sprite& sheet, int index)
{
    int size = sheet.height();
    int half_size = size / 2;
    int sx = (int)(x + 0.5f) - _cx - half_size;
    int sy = (int)(y + 0.5f) - _cy - half_size;
    _app->blend_partial_sprite(sx, sy, sheet, index * size, 0, size, size, 255);
}


void GamePlayState::draw_nmi(const V2f& position)
{
    float nmistatetime = NmiDuration - _nmitimer;
    uint8_t nmidarkalpha = 0;

    if (nmistatetime < NmiDarkInEnd)
    {
        float t = ease_in(nmistatetime / NmiDarkInEnd);
        nmidarkalpha = (uint8_t)(t * 255.0f);
    }
    else if (nmistatetime < NmiZapOutEnd)
    {
        nmidarkalpha = 255;
    }
    else if (nmistatetime < NmiDarkOutEnd)
    {
        float t = ease_out((nmistatetime - NmiZapOutEnd) / (NmiDarkOutEnd - NmiZapOutEnd));
        nmidarkalpha = (uint8_t)(t * 255.0f);
    }

    if (nmidarkalpha)
    {
        int nmi_sx = (int)(position.x + 0.5f) - _cx;
        int nmi_sy = (int)(position.y + 0.5f) - _cy;
        _app->blend_sprite(nmi_sx - _nmi_dark.width() / 2, nmi_sy - _nmi_dark.height() / 2, _nmi_dark, nmidarkalpha);
    }

    uint8_t nmizapalpha = 0;

    if (nmistatetime >= NmiDarkInEnd)
    {
        if (nmistatetime < NmiZapInEnd)
        {
            float t = ease_in((nmistatetime - NmiDarkInEnd) / (NmiZapInEnd - NmiDarkInEnd));
            nmizapalpha = (uint8_t)(t * 255.0f);
        }
        else if (nmistatetime < NmiZapOutStart)
        {
            nmizapalpha = 255;
        }
        else if (nmistatetime < NmiZapOutEnd)
        {
            float t = ease_out((nmistatetime - NmiZapOutStart) / (NmiZapOutEnd - NmiZapOutStart));
            nmizapalpha = (uint8_t)(t * 255.0f);
        }

        if (nmizapalpha)
        {
            int nmi_sx = (int)(position.x + 0.5f) - _cx;
            int nmi_sy = (int)(position.y + 0.5f) - _cy;
            _app->blend_sprite(nmi_sx - _nmi_shock.width() / 2, nmi_sy - _nmi_shock.height() / 2, _nmi_shock, nmizapalpha);
        }
    }
}


bool GamePlayState::check_collision(float x, float y, float half_size)
{
    int sx = (int)std::floor(x - half_size) / _tilemap.tile_size();
    int sy = (int)std::floor(y - half_size) / _tilemap.tile_size();
    int ex = (int)std::floor(x + half_size) / _tilemap.tile_size();
    int ey = (int)std::floor(y + half_size) / _tilemap.tile_size();

    if (sx < 0 || sx < 0 || ex >= _tilemap.width() || ey >= _tilemap.height())
    {
        return true;
    }

    for (int tx = sx; tx <= ex; ++tx)
    {
        for (int ty = sy; ty <= ey; ++ty)
        {
            if (!_tilemap.walkable((uint16_t)tx, (uint16_t)ty))
            {
                return true;
            }
        }
    }

    return false;
}


} // namespace Bootstrap
