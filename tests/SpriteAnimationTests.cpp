#include <framework/SpriteAnimation.h>

#include <cmath>
#include <iostream>
#include <limits>

#define CHECK(_condition) do { if (!(_condition)) { std::cerr << "Line " << __LINE__ << '\n'; return 1; } } while (false)

int main()
{
    SpriteAnimation animation;
    CHECK(!animation.GetCurrentFrame().IsValid());
    animation.Play();
    animation.Update(1.0);
    CHECK(!animation.IsPlaying());
    CHECK(!animation.Configure({}));
    CHECK(animation.ConfigureStrip({0, 0, 16, 16}, 4, 0.125));
    CHECK(animation.GetFrameIndex() == 0 && animation.GetCurrentFrame().x == 0);
    CHECK(!animation.IsPlaying() && !animation.IsFinished());
    animation.Play();
    animation.Update(0.125);
    CHECK(animation.GetFrameIndex() == 1 && animation.GetCurrentFrame().x == 16);
    animation.Play();
    animation.Update(0.25);
    CHECK(animation.GetFrameIndex() == 3);
    animation.Update(0.125);
    CHECK(animation.GetFrameIndex() == 0);
    animation.Update(1000000.375);
    CHECK(animation.GetFrameIndex() == 3);
    animation.Stop();
    animation.Update(1.0);
    CHECK(animation.GetFrameIndex() == 3 && !animation.IsPlaying());
    animation.Play();
    animation.Update(0.125);
    CHECK(animation.GetFrameIndex() == 0);

    SpriteAnimation other = animation;
    other.Update(0.25);
    CHECK(other.GetFrameIndex() == 2 && animation.GetFrameIndex() == 0);
    CHECK(!animation.ConfigureStrip({0, 0, 16, 16}, 0, 0.07));
    CHECK(!animation.ConfigureStrip({0, 0, 16, 16}, 4, 0.0));
    CHECK(!animation.ConfigureStrip({std::numeric_limits<int>::max(), 0, 16, 16}, 4, 0.07));
    CHECK(!animation.Configure({{{-1, 0, 16, 16}, 0.07}}));
    CHECK(!animation.Configure({{{0, 0, 16, 16}, -1.0}}));
    CHECK(!animation.Configure({{{0, 0, 16, 16}, std::numeric_limits<double>::infinity()}}));
    CHECK(!animation.Configure({{{0, 0, 16, 16}, std::numeric_limits<double>::quiet_NaN()}}));
    CHECK(animation.IsPlaying() && animation.GetFrameIndex() == 0);
    animation.Update(-1.0);
    animation.Update(std::numeric_limits<double>::infinity());
    animation.Update(std::numeric_limits<double>::quiet_NaN());
    CHECK(animation.GetFrameIndex() == 0);

    CHECK(animation.Configure({{{16, 16, 16, 16}, 0.125}, {{0, 0, 16, 16}, 0.25}}, AnimationMode::Once));
    animation.Play();
    animation.Update(0.125);
    CHECK(animation.GetCurrentFrame().x == 0 && animation.GetFrameIndex() == 1 && !animation.IsFinished());
    animation.Update(0.125);
    CHECK(!animation.IsFinished());
    animation.Update(0.125);
    CHECK(animation.IsFinished() && !animation.IsPlaying() && animation.GetFrameIndex() == 1);
    animation.Play();
    animation.Update(10.0);
    CHECK(animation.IsFinished() && animation.GetFrameIndex() == 1);
    animation.Reset();
    CHECK(!animation.IsFinished() && !animation.IsPlaying() && animation.GetCurrentFrame().y == 16);
    animation.Play();
    animation.Update(1e100);
    CHECK(animation.IsFinished() && animation.GetFrameIndex() == 1);

    CHECK(animation.ConfigureStrip({0, 0, 16, 16}, 4, 0.07));
    animation.Play();
    animation.Update(0.07);
    CHECK(animation.GetFrameIndex() == 1);
    animation.Update(0.07);
    CHECK(animation.GetFrameIndex() == 2);
    animation.Update(0.14);
    CHECK(animation.GetFrameIndex() == 0);
    CHECK(animation.ConfigureStrip({0, 0, 16, 16}, 1, 0.07));
    animation.Play();
    animation.Update(1e100);
    CHECK(animation.GetFrameIndex() == 0 && animation.IsPlaying());

    const TextureRegion region{48, 0, 16, 16};
    CHECK(region.FitsWithin(64, 16));
    CHECK(!region.FitsWithin(63, 16));
    CHECK(!region.FitsWithin(64, 15));
    CHECK(!region.FitsWithin(-1, -1));
    return 0;
}
