#include "bootstrap.h"

#include "appstate.h"
#include "iappstate.h"
#include "splash.h"
#include "version.h"

#include <memory>
#include <string>

namespace Bootstrap
{

bool App::on_create()
{
    _states.reserve(AppState::Count);

    AppStatePtr state = std::make_unique<SplashState>();

    if (!state->on_init(this))
    {
        return false;
    }

    _states.insert(std::pair<AppState, AppStatePtr>(AppState::Splash, std::move(state)));

    _state = AppState::Count;
    _next_state = AppState::Splash;
    _active_state = nullptr;

    return true;
}

void App::on_destroy()
{
    if (_active_state)
    {
        _active_state->on_exit();
        _active_state = nullptr;
    }

    for (const auto& kvp : _states)
    {
        if (kvp.second)
        {
            kvp.second->on_destroy();
        }
    }

    _states.clear();
}

bool App::on_update(float delta)
{
    if (_next_state != AppState::Count)
    {
        if (_active_state)
        {
            _active_state->on_exit();
            _active_state = nullptr;
        }

        const auto iter = _states.find(_next_state);

        if (iter != _states.end())
        {
            _active_state = iter->second.get();
        }

        if (_active_state)
        {
            if (!_active_state->on_enter())
            {
                _active_state = nullptr;
                return false;
            }
        }

        _next_state = AppState::Count;
        delta = 0.0f;
    }

    if (_active_state)
    {
        if (!_active_state->on_update(delta))
        {
            _active_state->on_exit();
            _active_state = nullptr;
        }
    }

    return !!_active_state;
}

} // namespace Bootstrap

Bootstrap::App app;

int gli_main(int argc, char** argv)
{
    size_t size = std::snprintf(nullptr, 0, "Bootstrap (%s)", APP_VERSION) + 1;
    std::string title(size, '\0');
    std::snprintf(&title[0], size, "Bootstrap (%s)", APP_VERSION);

    if (app.initialize(title.c_str(), 640, 360, 2))
    {
        app.run();
    }

    return 0;
}