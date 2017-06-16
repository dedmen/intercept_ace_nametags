#include "nametagger.hpp"
#include <algorithm>
#include <sstream>
#include <stdlib.h>
#undef min
#undef max
#include "client\sqf\sqf.hpp"
#include <client/eventhandlers.hpp>

using namespace intercept;

namespace ace {
    namespace nametags {
        nametagger::nametagger() { _scale = 0.666; }

        nametagger::~nametagger() {}

        /*!
@brief Based on ace_nametags_fnc_onDraw3d
*/
        std::function<bool(object)> isSpeaking;
        void initIsSpeaking() {
            if (sqf::is_server()) {
                //If someone disconnects while speaking, reset their variable
                client::addMissionEventHandler<client::eventhandlers_mission::HandleDisconnect>([](object _disconnectedPlayer) {

                    if (static_cast<bool>(sqf::get_variable(_disconnectedPlayer, "ace_nametags_isSpeakingInGame", false))) {
                        sqf::set_variable(_disconnectedPlayer, "ace_nametags_isSpeakingInGame", false, true);
                    };
                });
            };

            if (!sqf::has_interface()) return;

            //Would need signal or RSQF
            //["unit", {
            //    //When player changes, make sure to reset old unit's variable
            //    params["", "_oldUnit"];
            //if ((!isNull _oldUnit) && {_oldUnit getVariable[QGVAR(isSpeakingInGame), false]}) then{
            //    _oldUnit setVariable[QGVAR(isSpeakingInGame), false, true];
            //};
            //}] call CBA_fnc_addPlayerEventHandler;

            if (sqf::is_class(sqf::config_entry() >> "CfgPatches" >> "acre_api")) {
                isSpeaking = [](object _unit) {
                    //return ([_unit] call acre_api_fnc_isSpeaking) && !static_cast<bool>(sqf::get_variable(_unit, "ACE_isUnconscious", false));
                    return false;
                };
            } else {
                if (sqf::is_class(sqf::config_entry() >> "CfgPatches" >> "task_force_radio")) {
                    isSpeaking = [](object _unit) {
                        return static_cast<bool>(sqf::get_variable(_unit, "tf_isSpeaking", false)) && !static_cast<bool>(sqf::get_variable(_unit, "ACE_isUnconscious", false));
                    };
                } else {
                    //No Radio Mod - Start a PFEH to watch the internal VON icon
                    //Note: class RscDisplayVoiceChat {idd = 55} - only present when talking

                    //[{
                    //    private["_oldSetting", "_newSetting"];
                    //    _oldSetting = ACE_player getVariable[QGVAR(isSpeakingInGame), false];
                    //    _newSetting = (!(isNull findDisplay 55));
                    //    if (!(_oldSetting isEqualTo _newSetting)) then{
                    //        ACE_player setVariable[QGVAR(isSpeakingInGame), _newSetting, true];
                    //    };
                    //}, 0.1, []] call CBA_fnc_addPerFrameHandler;
                    isSpeaking = [](object _unit) {
                        return static_cast<bool>(sqf::get_variable(_unit, "ace_nametags_isSpeakingInGame", false)) && !static_cast<bool>(sqf::get_variable(_unit, "ACE_isUnconscious", false));
                    };
                }
            }
        }

        void nametagger::pre_init() {
            initIsSpeaking();
        }

        void nametagger::on_frame() {
            // Don't show nametags in spectator or if RscDisplayMPInterrupt is open
            auto ACE_Player = static_cast<object>(sqf::get_variable(sqf::mission_namespace(), "ACE_player"));

            if (ACE_Player.is_nil() || sqf::is_null(ACE_Player) || !sqf::alive(ACE_Player) || !sqf::is_null(sqf::find_display(49))) return;

            //#TODO cache again
            auto _drawName = true;
            auto _enabledTagsNearby = false;
            auto _enabledTagsCursor = false;

            auto showSoundWaves = static_cast<int>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_showSoundWaves"));
            auto showPlayerNames = static_cast<int>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_showPlayerNames"));
            switch (showPlayerNames) {
                case 0: {
                    // Player names Disabled [Note: this should be unreachable as the drawEH will be removed]
                    _drawName = false;
                    _enabledTagsNearby = showSoundWaves == 2;
                };
                case 1: {
                    // Player names Enabled
                    _enabledTagsNearby = true;
                };
                case 2: {
                    // Player names Only cursor
                    _enabledTagsNearby = showSoundWaves == 2;
                    _enabledTagsCursor = true;
                };
                case 3: {
                    // Player names Only Keypress
                    _enabledTagsNearby = showSoundWaves == 2;  // non-cached: || _onKeyPressAlphaMax) > 0
                };
                case 4: {
                    // Player names Only Cursor and Keypress
                    _enabledTagsNearby = showSoundWaves == 2;
                    // non-cached: _enabledTagsCursor = _onKeyPressAlphaMax > 0;
                };
                case 5: {
                    // Fade on border
                    _enabledTagsNearby = true;
                }
            }
            static code ambientBrightness = sqf::get_variable(sqf::mission_namespace(), "ACE_common_ambientBrightness");
            static code isPlayer = sqf::get_variable(sqf::mission_namespace(), "ACE_common_isPlayer");

            auto _ambientBrightness = std::clamp(static_cast<float>(sqf::call(ambientBrightness, auto_array<game_value>())) + sqf::current_vision_mode(ACE_Player) ? 0.4f : 0.f, 0.f, 1.f);
            auto _maxDistance = _ambientBrightness * static_cast<float>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_PlayerNamesViewDistance"));
            auto _drawRank = static_cast<int>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_showPlayerNames"));
            auto showNamesTime = static_cast<float>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_showNamesTime"));
            auto showNamesForAI = static_cast<int>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_showNamesForAI"));
            auto playerNamesMaxAlpha = static_cast<float>(sqf::get_variable(sqf::mission_namespace(), "ACE_nametags_playerNamesMaxAlpha"));
            auto CBA_missionTime = static_cast<float>(sqf::get_variable(sqf::mission_namespace(), "CBA_missionTime"));

            auto _onKeyPressAlphaMax = 1.f;
            if (showPlayerNames == 3) {
                _onKeyPressAlphaMax = 2.f + (showNamesTime - CBA_missionTime);
                _enabledTagsNearby = _enabledTagsNearby || _onKeyPressAlphaMax > 0;
            };
            if (showPlayerNames == 4) {
                _onKeyPressAlphaMax = 2.f + (showNamesTime - CBA_missionTime);
                _enabledTagsCursor = _onKeyPressAlphaMax > 0;
            };

            auto _camPosAGL = sqf::position_camera_to_world({0, 0, 0});
            //if !((_camPosAGL select 0) isEqualType 0) exitWith {}; // handle RHS / bugged vehicle slots

            auto _camPosASL = sqf::agl_to_asl(_camPosAGL);

            // Show nametag for the unit behind the cursor or its commander
            if (_enabledTagsCursor) {
                auto _target = sqf::cursor_target();
                if (!sqf::is_kind_of(_target, "CAManBase")) {
                    // When cursorTarget is on a vehicle show the nametag for the commander.
                    auto uavUnits = sqf::all_units_uav();
                    if (std::find(uavUnits.begin(), uavUnits.end(), _target) == uavUnits.end()) {
                        _target = sqf::effective_commander(_target);
                    } else {
                        _target = sqf::obj_null();
                    };
                };
                if (sqf::is_null(_target)) return;

                if (_target != ACE_Player && sqf::get_side(sqf::get_group(_target)) == sqf::get_side(sqf::get_group(ACE_Player)) && (showNamesForAI || static_cast<bool>(sqf::call(isPlayer, {_target}))) && sqf::line_intersects_surfaces(_camPosASL, sqf::eye_pos(_target), ACE_Player, _target).size() == 0 && !sqf::is_object_hidden(_target)) {
                    auto _distance = sqf::distance(std::variant<object, vector3>(ACE_Player), _target);

                    auto _drawSoundwave = (showSoundWaves > 0) && isSpeaking(_target);
                    // Alpha:
                    // - base value determined by GVAR(playerNamesMaxAlpha)
                    // - decreases when _distance > _maxDistance
                    // - increases when the unit is speaking
                    // - it's clamped by the value of _onKeyPressAlphaMax

                    auto _alpha = std::min(std::min(1.f + (_drawSoundwave ? 0.2f : 0.f) - 0.2f * (_distance - _maxDistance), 1.f) * playerNamesMaxAlpha, _onKeyPressAlphaMax);

                    if (_alpha > 0) {
                        //#TODO enum broken. Should correctly differentiate between what to draw.
                        draw_nametag(_target, _alpha, _distance * 0.026, name_rank, true);
                        //[ACE_player, _target, _alpha, _distance * 0.026, _drawName, _drawRank, _drawSoundwave] call FUNC(drawNameTagIcon);
                    };
                };
            };

            // Show nametags for nearby units
            if (_enabledTagsNearby) {
                // Find valid targets and cache them
                auto getTargets = [&]() {
                    auto _nearMen = sqf::near_objects(_camPosAGL, "CAManBase", _maxDistance + 7);
                    std::remove_if(_nearMen.begin(), _nearMen.end(), [&](object& unit) {
                        return unit == ACE_Player || sqf::get_side(sqf::get_group(unit)) != sqf::get_side(sqf::get_group(ACE_Player)) || (!showNamesForAI && !static_cast<bool>(sqf::call(isPlayer, {unit}))) || !sqf::line_intersects_surfaces(_camPosASL, sqf::eye_pos(unit), ACE_Player, unit).size() == 0 || sqf::is_object_hidden(unit);
                    });

                    std::vector<object> _crewMen;
                    if (sqf::vehicle(ACE_Player) != ACE_Player) {
                        _crewMen = sqf::crew(sqf::vehicle(ACE_Player));

                        std::remove_if(_crewMen.begin(), _crewMen.end(), [&](object& unit) {
                            return unit == ACE_Player || sqf::get_side(sqf::get_group(unit)) != sqf::get_side(sqf::get_group(ACE_Player)) || (!showNamesForAI && !static_cast<bool>(sqf::call(isPlayer, {unit}))) || !sqf::line_intersects_surfaces(_camPosASL, sqf::eye_pos(unit), ACE_Player, unit).size() == 0 || sqf::is_object_hidden(unit);
                        });
                    };
                    _nearMen.insert(_nearMen.end(), _crewMen.begin(), _crewMen.end());
                    return _nearMen;
                };

                //#TODO cache getTargets
                for (auto& _target : getTargets()) {
                    if (sqf::is_null(_target)) continue;
                    auto _drawSoundwave = (showSoundWaves > 0) && isSpeaking(_target);
                    if (_enabledTagsCursor && !_drawSoundwave) return;  // (Cursor Only && showSoundWaves==2) - quick exit

                    auto _relPos = sqf::visible_position_asl(_target) - _camPosASL;
                    auto _distance = _relPos.magnitude();

                    // Fade on border
                    float _centerOffsetFactor = 1.f;
                    if ((showPlayerNames) == 5) {
                        auto _screenPos = sqf::world_to_screen(sqf::model_to_world(_target, sqf::selection_positon(_target, "head")));
                        if (!_screenPos.zero_distance()) {
                            // Distance from center / half of screen width
                            _centerOffsetFactor = 1 - _screenPos.distance({0.5f, 0.5f}) / (sqf::safe_zone_w() / 3);
                        } else {
                            _centerOffsetFactor = 0;
                        };
                    };

                    auto _alphaMax = _onKeyPressAlphaMax;
                    if (((showSoundWaves) == 2) && _drawSoundwave) {
                        _drawName = _drawSoundwave;
                        _drawRank = false;
                        _alphaMax = 1;
                    };
                    // Alpha:
                    // - base value determined by GVAR(playerNamesMaxAlpha)
                    // - decreases when _distance > _maxDistance
                    // - increases when the unit is speaking
                    // - it's clamped by the value of _onKeyPressAlphaMax unless soundwaves are forced on and the unit is talking
                    auto _alpha = std::min(std::min(1.f + (_drawSoundwave ? 0.2f : 0.f) - 0.2f * (_distance - _maxDistance), 1.f) * playerNamesMaxAlpha * _centerOffsetFactor, _alphaMax);

                    if (_alpha > 0) {
                        //#TODO enum broken. Should correctly differentiate between what to draw.
                        draw_nametag(_target, _alpha, _distance * 0.026, name_rank, true);
                        //[ACE_player, _target, _alpha, _distance * 0.026, _drawName, _drawRank, _drawSoundwave] call FUNC(drawNameTagIcon);
                    };
                };
            };
        }

        /*!
@brief Returns the name of a unit. Based on ace_common_fnc_getName
*/
        std::string nametagger::get_name(types::object& unit_,
                                         bool _show_effective_on_vehicle) {
            if (sqf::is_kind_of(unit_, "CAManBase")) {
                return sqf::get_variable(unit_, "ACE_NameRaw", game_value(""));
            } else {
                if (_show_effective_on_vehicle) {
                    return get_name(sqf::effective_commander(unit_), false);
                } else {
                    sqf::config_entry name_reader;
                    return sqf::get_text(name_reader >> "CfgVehicles" >> sqf::type_of(unit_) >> "displayName");
                }
            };
        }

        /*!
@brief Draw a nametag for a unit. Based on ace_nametags_fnc_drawNameTagIcon
*/
        void nametagger::draw_nametag(types::object& unit_, float alpha_,
                                      float heightOffset_, icon_type icon_type_,
                                      bool is_player_group_) {
            if (icon_type_ == none) {
                return;
            }
            if (sqf::is_object_hidden(unit_)) {
                return;
            }

            float size = 0;
            float alpha = alpha_;
            std::stringstream ss;
            if (icon_type_ == name_speak || icon_type_ == speak) {
                ss << "\\z\\ace\\addons\\nametags\\ui\\soundwave";
                ss << (int)(rand() % 10);
                ss << ".paa";
                size = 1.0f;
                alpha = std::max(alpha, 0.2f) + 0.2f;
            } else if (icon_type_ == name_rank) {
                ss << "\\A3\\Ui_f\\data\\GUI\\Cfg\\Ranks\\";
                ss << sqf::rank(unit_);
                ss << "_gs.paa";
                size = 1.0f;
            }
            std::string icon(ss.str());

            if (alpha <= 0) {
                return;
            }

            std::string unit_name("");
            if (icon_type_ == name || icon_type_ == name_rank || icon_type_ == name_speak) {
                unit_name = get_name(unit_, true);
            };

            // sqf::rv_color
            // color(sqf::__helpers::__convert_to_numbers_vector(sqf::get_variable(sqf::mission_namespace(),
            // "ace_nametags_defaultNametagColor")));
            rv_color color{0.77f, 0.51f, 0.08f, 1.0f};
            if (is_player_group_) {
                std::string team(sqf::assigned_team(unit_));
                if (team.compare("MAIN") == 0) {
                    color.red = 1;
                    color.green = 1;
                    color.blue = 1;
                } else if (team.compare("RED") == 0) {
                    color.red = 1;
                    color.green = 0;
                    color.blue = 0;
                } else if (team.compare("GREEN") == 0) {
                    color.red = 0;
                    color.green = 1;
                    color.blue = 0;
                } else if (team.compare("BLUE") == 0) {
                    color.red = 0;
                    color.green = 0;
                    color.blue = 1;
                } else if (team.compare("YELLOW") == 0) {
                    color.red = 1;
                    color.green = 1;
                    color.blue = 0;
                };
            }
            color.alpha *= alpha;

            vector3 pos(sqf::asl_to_agl(sqf::eye_pos(unit_)));
            pos.z += heightOffset_ + 0.3f;

            sqf::draw_icon_3d(icon, color, pos, size * _scale, size * _scale, 0,
                              unit_name, 2, 0.05f * _scale, "PuristaMedium");
        }
    }  // namespace nametags
}  // namespace ace
