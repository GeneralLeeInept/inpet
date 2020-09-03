#pragma once

#include "iappstate.h"

#include <gli.h>

namespace Bootstrap
{

class PuzzleState : public IAppState
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
    enum Sprite
    {
        Board,
        PiecesSheet,
        GoButtonPressed,
        VerifyingDialog,
        ValidationFailed,
        Count
    };

    struct ControlLineDef
    {
        int line;
        int behavior;
        int target;
    };

    void draw_tile(const ControlLineDef& linedef, int x, int y, bool center);
    size_t select_from_bag(int x, int y);
    void remove_from_solution(int x, int y);
    bool add_to_solution(int x, int y, size_t def);

    std::vector<ControlLineDef> _linedefs;
    std::vector<size_t> _solution;
    gli::Sprite _sprites[Sprite::Count];
    App* _app;
    size_t _dragging;
    bool _go_pressed;
    float _verifying;
    float _failed;
};

} // namespace Bootstrap
