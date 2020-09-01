#pragma once

#include "iappstate.h"
#include "tilemap.h"

#include <gli.h>

namespace Bootstrap
{

class GamePlayState : public IAppState
{
public:
    bool on_init(App* app) override;
    void on_destroy() override;
    bool on_enter() override;
    void on_exit() override;
    void on_suspend() override;
    void on_resume() override;
    bool on_update(float delta) override;

private:
    void draw_register(uint8_t reg, int x, int y, int color);

    App* _app{};
    TileMap _tilemap;
    gli::Sprite _player;
    gli::Sprite _status_panel;
    gli::Sprite _leds;
    float _px;
    float _py;
    float _pvx;
    float _pvy;
    uint8_t _dbus;
    uint8_t _abus_hi;
    uint8_t _abus_lo;
    uint8_t _a_reg;
    uint8_t _x_reg;
    uint8_t _y_reg;
};

} // namespace Bootstrap
