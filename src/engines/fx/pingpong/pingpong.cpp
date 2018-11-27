#include "pingpong.hpp"

#include "core/ui/vector_graphics.hpp"

#include "util/iterator.hpp"
#include "util/utility.hpp"

#include "pingpong.faust.hpp"

namespace otto::engines {

  using namespace ui;
  using namespace ui::vg;

  struct PingpongScreen : EngineScreen<Pingpong> {
    void draw(Canvas& ctx) override;
    bool keypress(Key key) override;
    void rotary(RotaryEvent e) override;

    using EngineScreen<Pingpong>::EngineScreen;
  };

  Pingpong::Pingpong()
    : EffectEngine("PingPong", props, std::make_unique<PingpongScreen>(this)),
      faust_(std::make_unique<faust_pingpong>(), props)
  {}


  audio::ProcessData<2> Pingpong::process(audio::ProcessData<2> data)
  {
    return faust_.process(data);
  }

  // SCREEN //

  void PingpongScreen::rotary(ui::RotaryEvent ev)
  {
    auto& props = engine.props;
    switch (ev.rotary) {
    case Rotary::Blue: props.delaytime.step(ev.clicks); break;
    case Rotary::Green: props.feedback.step(ev.clicks); break;
    case Rotary::White: props.tone.step(ev.clicks); break;
    case Rotary::Red: props.spread.step(ev.clicks); break;
    }
  }

  bool PingpongScreen::keypress(ui::Key key)
  {
    switch (key) {
    case ui::Key::blue_click: [[fallthrough]];
    case ui::Key::green_click: [[fallthrough]];
    case ui::Key::white_click: [[fallthrough]];
    case ui::Key::red_click:
      engine.props.bpm_follow = !engine.props.bpm_follow; break;
    default: return false; ;
    }
    return true;
  }

  void PingpongScreen::draw(ui::vg::Canvas& ctx)
  {
    using namespace ui::vg;
    using util::math::vec;

    ctx.font(Fonts::Norm, 35);

    constexpr float x_pad = 30;
    constexpr float y_pad = 50;
    constexpr float space = (height - 2.f * y_pad) / 3.f;

    int x_spacing_min = width/25;
    int x_scaling = 50;
    int y_base = height/2;
    int line_length = 1;
    ctx.lineWidth(6.0f);
    int x_pos;

    int feedbackLimit = floor(20.0*engine.props.feedback);
    int y_height_top = 100;
    int y_height_bot = 100;
    float dim_scale_top = engine.props.feedback - 0.01 - 0.2*max(0.0, 0.5 - engine.props.tone);
    float dim_scale_bot = engine.props.feedback - 0.01 - 0.2*max(0.0, engine.props.tone - 0.5);
    for (int i=0;i<20;i++) {
      ctx.beginPath();
      x_pos = x_pad + i*(x_spacing_min+x_scaling*engine.props.delaytime);
      ctx.moveTo(x_pos, y_base - y_height_top);
      ctx.lineTo(x_pos, y_base + y_height_bot);
      ctx.stroke(Colours::Gray50);

      ctx.beginPath();
      engine.props.delay_level[i].level.refresh_links();
      ctx.moveTo(x_pos, y_base - y_height_top*engine.props.delay_level[i].level);
      ctx.lineTo(x_pos, y_base + y_height_bot*engine.props.delay_level[i].level);
      ctx.stroke(Colours::Blue);
      y_height_top *= dim_scale_top;
      y_height_bot *= dim_scale_bot;
    }


  }

} // namespace otto::engines
