#include "item_tname.h"

#include <set>
#include <string>
#include <vector>

#include "avatar.h"
#include "cata_utility.h"
#include "color.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "debug.h"
#include "enums.h"
#include "fault.h"
#include "flag.h"
#include "flat_set.h"
#include "item.h"
#include "item_category.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "itype.h"
#include "map.h"
#include "options.h"
#include "point.h"
#include "recipe.h"
#include "relic.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"


static const itype_id itype_barrel_small( "barrel_small" );
static const itype_id itype_disassembly( "disassembly" );

static const skill_id skill_survival( "survival" );

std::string tname::faults( item const &it, unsigned int /* quantity */,
                           segment_bitset const &/* segments */ )
{
    std::string damtext;
    // add first prefix if item has a fault that defines a prefix (prioritize?)
    for( const fault_id &f : it.faults ) {
        const std::string prefix = f->item_prefix();
        if( !prefix.empty() ) {
            damtext = prefix + " ";
            break;
        }
    }

    return damtext;
}

std::string tname::dirt_symbol( item const &it, unsigned int /* quantity */,
                                segment_bitset const &/* segments */ )
{
    return it.dirt_symbol();
}

std::string tname::overheat_symbol( item const &it, unsigned int /* quantity */,
                                    segment_bitset const &/* segments */ )
{
    return it.overheat_symbol();
}

std::string tname::pre_asterisk( item const &it, unsigned int /* quantity */,
                                 segment_bitset const &/* segments */ )
{
    if( it.is_favorite && get_option<std::string>( "ASTERISK_POSITION" ) == "prefix" ) {
        return _( "* " ); // Display asterisk for favorite items, before item's name
    }
    return {};
}

std::string tname::durability( item const &it, unsigned int /* quantity */,
                               segment_bitset const &/* segments */ )
{
    const std::string item_health_option = get_option<std::string>( "ITEM_HEALTH" );
    const bool show_bars = item_health_option == "both" || item_health_option == "bars";
    if( !it.is_null() &&
        ( it.damage() != 0 || ( it.degradation() > 0 && it.degradation() >= it.max_damage() / 5 ) ||
          ( show_bars && it.is_armor() ) ) ) {
        return it.durability_indicator();
    }
    return {};
}

std::string tname::engine_displacement( item const &it, unsigned int /* quantity */,
                                        segment_bitset const &/* segments */ )
{
    if( it.is_engine() && it.engine_displacement() > 0 ) {
        return string_format( pgettext( "vehicle adjective", "%gL " ),
                              it.engine_displacement() / 100.0f );
    }
    return {};
}

std::string tname::wheel_diameter( item const &it, unsigned int /* quantity */,
                                   segment_bitset const &/* segments */ )
{
    if( it.is_wheel() && it.type->wheel->diameter > 0 ) {
        return string_format( pgettext( "vehicle adjective", "%d\" " ), it.type->wheel->diameter );
    }
    return {};
}

std::string tname::burn( item const &it, unsigned int /* quantity */,
                         segment_bitset const &/* segments */ )
{
    if( !it.made_of_from_type( phase_id::LIQUID ) ) {
        if( it.volume() >= 1_liter && it.burnt * 125_ml >= it.volume() ) {
            return pgettext( "burnt adjective", "badly burnt " );
        }
        if( it.burnt > 0 ) {
            return pgettext( "burnt adjective", "burnt " );
        }
    }
    return {};
}

std::string tname::label( item const &it, unsigned int quantity, segment_bitset const &segments )
{
    if( !it.is_craft() ) {
        return it.label( quantity, segments[tname::segments::VARIANT],
                         ( segments & tname_conditional ) == tname_conditional,
                         segments[tname::segments::CORPSE] );
    }
    return {};
}

std::string tname::mods( item const &it, unsigned int /* quantity */,
                         segment_bitset const &/* segments */ )
{
    int amt = 0;
    if( it.is_armor() && it.has_clothing_mod() ) {
        amt++;
    }
    if( ( ( it.is_gun() || it.is_tool() || it.is_magazine() ) && !it.is_power_armor() ) ||
        it.get_contents().has_additional_pockets() ) {

        for( const item *mod : it.is_gun() ? it.gunmods() : it.toolmods() ) {
            if( !it.type->gun || !it.type->gun->built_in_mods.count( mod->typeId() ) ||
                !it.type->gun->default_mods.count( mod->typeId() ) ) {
                amt++;
            }
        }
        amt += it.get_contents().get_added_pockets().size();
    }
    if( amt > 0 ) {
        return string_format( "+%d", amt );
    }
    return {};
}

std::string tname::craft( item const &it, unsigned int /* quantity */,
                          segment_bitset const &/* segments */ )
{
    if( it.is_craft() ) {
        std::string maintext;
        if( it.typeId() == itype_disassembly ) {
            maintext = string_format( _( "in progress disassembly of %s" ),
                                      it.get_making().result_name() );
        } else {
            maintext = string_format( _( "in progress %s" ), it.get_making().result_name() );
        }
        if( it.charges > 1 ) {
            maintext += string_format( " (%d)", it.charges );
        }
        const int percent_progress = it.item_counter / 100000;
        return string_format( "%s (%d%%)", maintext, percent_progress );
    }
    return {};
}

std::string tname::wbl_mark( item const &it, unsigned int /* quantity */,
                             segment_bitset const &/* segments */ )
{
    std::vector<const item_pocket *> pkts = it.get_all_contained_pockets();
    bool wl = false;
    bool bl = false;
    bool player_wbl = false;
    for( item_pocket const *pkt : pkts ) {
        bool const wl_ = !pkt->settings.get_item_whitelist().empty() ||
                         !pkt->settings.get_category_whitelist().empty();
        bool const bl_ = !pkt->settings.get_item_blacklist().empty() ||
                         !pkt->settings.get_category_blacklist().empty();
        player_wbl |= ( wl_ || bl_ ) && pkt->settings.was_edited();
        wl |= wl_;
        bl |= bl_;
    }
    return
        wl || bl ? colorize( "⁺", player_wbl ? c_light_gray : c_dark_gray ) : std::string();
}

std::string tname::contents( item const &it, unsigned int /* quantity */,
                             segment_bitset const &segments )
{
    item_contents const &contents = it.get_contents();
    if( item::aggregate_t aggi = it.aggregated_contents();
        segments[tname::segments::CONTENTS_FULL] && aggi ) {

        const item &contents_item = *aggi.header;
        const unsigned aggi_count = contents_item.count_by_charges() && contents_item.charges > 1
                                    ? contents_item.charges
                                    : aggi.count;
        const unsigned total_count = contents_item.count_by_charges() && contents_item.charges > 1
                                     ? contents_item.charges
                                     : contents.num_item_stacks();

        // without full contents for nested items to prevent excessively long names
        segment_bitset abrev( aggi.info.bits );
        abrev.set( tname::segments::CONTENTS_FULL, false );
        abrev.set( tname::segments::CONTENTS_ABREV, aggi_count == 1 );
        abrev.set( tname::segments::CATEGORY,
                   abrev[tname::segments::CATEGORY] && !abrev[tname::segments::TYPE] );
        std::string const contents_tname = contents_item.tname( aggi_count, abrev );
        std::string const ctnc = abrev[tname::segments::CATEGORY]
                                 ? contents_tname
                                 : colorize( contents_tname, contents_item.color_in_inventory() );

        // Don't append an item count for single items, or items that are ammo-exclusive
        // (eg: quivers), as they format their own counts.
        if( total_count > 1 && it.ammo_types().empty() ) {
            if( total_count == aggi_count ) {
                return string_format( " > %1$zd %2$s", total_count, ctnc );
            }
            return string_format(
                       //~ [container item name] " > [count] [type] / [total] items"
                       npgettext( "item name", " > %1$zd %2$s / %3$zd item", " > %1$zd %2$s / %3$zd items", total_count ),
                       aggi_count, ctnc, total_count );
        }
        return string_format( " > %1$s", ctnc );

    } else if( segments[tname::segments::CONTENTS_ABREV] && !contents.empty_container() &&
               contents.num_item_stacks() != 0 ) {
        std::string const suffix =
            npgettext( "item name",
                       //~ [container item name] " > [count] item"
                       " > %1$zd item", " > %1$zd items", contents.num_item_stacks() );
        return string_format( suffix, contents.num_item_stacks() );
    }
    return {};
}

std::string tname::food_traits( item const &it, unsigned int /* quantity */,
                                segment_bitset const &/* segments */ )
{
    if( it.is_food() ) {
        if( it.has_flag( flag_HIDDEN_POISON ) &&
            get_avatar().get_greater_skill_or_knowledge_level( skill_survival ) >= 3 ) {
            return _( " (poisonous)" );
        } else if( it.has_flag( flag_HIDDEN_HALLU ) &&
                   get_avatar().get_greater_skill_or_knowledge_level( skill_survival ) >= 5 ) {
            return _( " (hallucinogenic)" );
        }
    }
    return {};
}

std::string tname::location_hint( item const &it, unsigned int /* quantity */,
                                  segment_bitset const &/* segments */ )
{
    if( it.has_var( "spawn_location_omt" ) ) {
        tripoint_abs_omt loc( it.get_var( "spawn_location_omt", tripoint_zero ) );
        tripoint_abs_omt player_loc( ms_to_omt_copy( get_map().getabs( get_avatar().pos() ) ) );
        int dist = rl_dist( player_loc, loc );
        if( dist < 1 ) {
            return _( " (from here)" );
        } else if( dist < 6 ) {
            return _( " (from nearby)" );
        } else if( dist < 30 ) {
            return _( " (from this area)" );
        }
        return _( " (from far away)" );
    }
    return {};
}

std::string tname::ethereal( item const &it, unsigned int /* quantity */,
                             segment_bitset const &/* segments */ )
{
    if( it.ethereal ) {
        return string_format( _( " (%s turns)" ), it.get_var( "ethereal" ) );
    }
    return {};
}

std::string tname::food_status( item const &it, unsigned int /* quantity */,
                                segment_bitset const &/* segments */ )
{
    std::string tagtext;
    if( it.goes_bad() || it.is_food() ) {
        if( it.has_own_flag( flag_DIRTY ) ) {
            tagtext += _( " (dirty)" );
        } else if( it.rotten() ) {
            tagtext += _( " (rotten)" );
        } else if( it.has_flag( flag_MUSHY ) ) {
            tagtext += _( " (mushy)" );
        } else if( it.is_going_bad() ) {
            tagtext += _( " (old)" );
        } else if( it.is_fresh() ) {
            tagtext += _( " (fresh)" );
        }
    }
    return tagtext;
}

std::string tname::food_irradiated( item const &it, unsigned int /* quantity */,
                                    segment_bitset const &/* segments */ )
{
    if( it.goes_bad() ) {
        if( it.has_own_flag( flag_IRRADIATED ) ) {
            return _( " (irradiated)" );
        }
    }
    return {};
}

std::string tname::temperature( item const &it, unsigned int /* quantity */,
                                segment_bitset const &/* segments */ )
{
    std::string ret;
    if( it.has_temperature() ) {
        if( it.has_flag( flag_HOT ) ) {
            ret = _( " (hot)" );
        }
        if( it.has_flag( flag_COLD ) ) {
            ret = _( " (cold)" );
        }
        if( it.has_flag( flag_FROZEN ) ) {
            ret += _( " (frozen)" );
        } else if( it.has_flag( flag_MELTS ) ) {
            ret += _( " (melted)" ); // he melted
        }
    }
    return ret;
}

std::string tname::clothing_size( item const &it, unsigned int /* quantity */,
                                  segment_bitset const &/* segments */ )
{
    const item::sizing sizing_level = it.get_sizing( get_avatar() );

    if( sizing_level == item::sizing::human_sized_small_char ) {
        return _( " (too big)" );
    } else if( sizing_level == item::sizing::big_sized_small_char ) {
        return _( " (huge!)" );
    } else if( sizing_level == item::sizing::human_sized_big_char ||
               sizing_level == item::sizing::small_sized_human_char ) {
        return _( " (too small)" );
    } else if( sizing_level == item::sizing::small_sized_big_char ) {
        return _( " (tiny!)" );
    } else if( !it.has_flag( flag_FIT ) && it.has_flag( flag_VARSIZE ) ) {
        return _( " (poor fit)" );
    }
    return {};
}

std::string tname::filthy( item const &it, unsigned int /* quantity */,
                           segment_bitset const &/* segments */ )
{
    if( it.is_filthy() ) {
        return _( " (filthy)" );
    }
    return {};
}

std::string tname::tags( item const &it, unsigned int /* quantity */,
                         segment_bitset const &/* segments */ )
{
    std::string ret;
    if( it.is_gun() && ( it.has_flag( flag_COLLAPSED_STOCK ) || it.has_flag( flag_FOLDED_STOCK ) ) ) {
        ret += _( " (folded)" );
    }
    if( it.has_flag( flag_RADIO_MOD ) ) {
        ret += _( " (radio:" );
        if( it.has_flag( flag_RADIOSIGNAL_1 ) ) {
            ret += pgettext( "The radio mod is associated with the [R]ed button.", "R)" );
        } else if( it.has_flag( flag_RADIOSIGNAL_2 ) ) {
            ret += pgettext( "The radio mod is associated with the [B]lue button.", "B)" );
        } else if( it.has_flag( flag_RADIOSIGNAL_3 ) ) {
            ret += pgettext( "The radio mod is associated with the [G]reen button.", "G)" );
        } else {
            debugmsg( "Why is the radio neither red, blue, nor green?" );
            ret += "?)";
        }
    }
    if( it.active && ( it.has_flag( flag_WATER_EXTINGUISH ) || it.has_flag( flag_LITCIG ) ) ) {
        ret += _( " (lit)" );
    }
    return ret;
}

std::string tname::vars( item const &it, unsigned int /* quantity */,
                         segment_bitset const &/* segments */ )
{
    std::string ret;
    if( it.has_var( "NANOFAB_ITEM_ID" ) ) {
        if( it.has_flag( flag_NANOFAB_TEMPLATE_SINGLE_USE ) ) {
            //~ Single-use descriptor for nanofab templates. %s = name of resulting item. The leading space is intentional.
            ret += string_format( _( " (SINGLE USE %s)" ),
                                  item::nname( itype_id( it.get_var( "NANOFAB_ITEM_ID" ) ) ) );
        }
        ret += string_format( " (%s)", item::nname( itype_id( it.get_var( "NANOFAB_ITEM_ID" ) ) ) );
    }
    if( it.already_used_by_player( get_avatar() ) ) {
        ret += _( " (used)" );
    }
    if( it.has_flag( flag_IS_UPS ) && it.get_var( "cable" ) == "plugged_in" ) {
        ret += _( " (plugged in)" );
    }
    return ret;
}

std::string tname::broken( item const &it, unsigned int /* quantity */,
                           segment_bitset const &/* segments */ )
{
    if( it.is_broken() ) {
        return _( " (broken)" );
    }
    return {};
}

std::string tname::cbm_status( item const &it, unsigned int /* quantity */,
                               segment_bitset const &/* segments */ )
{
    if( it.is_bionic() && !it.has_flag( flag_NO_PACKED ) ) {
        if( !it.has_flag( flag_NO_STERILE ) ) {
            return _( " (sterile)" );
        }
        return _( " (packed)" );
    }
    return {};
}

std::string tname::ups( item const &it, unsigned int /* quantity */,
                        segment_bitset const &/* segments */ )
{
    if( it.is_tool() && it.has_flag( flag_USE_UPS ) ) {
        return _( " (UPS)" );
    }
    return {};
}

std::string tname::wetness( item const &it, unsigned int /* quantity */,
                            segment_bitset const &/* segments */ )
{
    if( ( it.wetness > 0 ) || it.has_flag( flag_WET ) ) {
        return _( " (wet)" );
    }
    return {};
}

std::string tname::active( item const &it, unsigned int /* quantity */,
                           segment_bitset const &/* segments */ )
{
    if( it.active && !it.has_temperature() && !string_ends_with( it.typeId().str(), "_on" ) ) {
        // Usually the items whose ids end in "_on" have the "active" or "on" string already contained
        // in their name, also food is active while it rots.
        return _( " (active)" );
    }
    return {};
}

std::string tname::sealed( item const &it, unsigned int /* quantity */,
                           segment_bitset const &/* segments */ )
{
    if( it.all_pockets_sealed() ) {
        return _( " (sealed)" );
    } else if( it.any_pockets_sealed() ) {
        return _( " (part sealed)" );
    }
    return {};
}

std::string tname::post_asterisk( item const &it, unsigned int /* quantity */,
                                  segment_bitset const &/* segments */ )
{
    if( it.is_favorite && get_option<std::string>( "ASTERISK_POSITION" ) == "suffix" ) {
        return _( " *" );
    }
    return {};
}

std::string tname::weapon_mods( item const &it, unsigned int /* quantity */,
                                segment_bitset const &/* segments */ )
{
    std::string modtext;
    if( it.gunmod_find( itype_barrel_small ) != nullptr ) {
        modtext += _( "sawn-off " );
    }
    if( it.is_gun() && it.has_flag( flag_REMOVED_STOCK ) ) {
        modtext += _( "pistol " );
    }
    if( it.has_flag( flag_DIAMOND ) ) {
        modtext += std::string( pgettext( "Adjective, as in diamond katana", "diamond" ) ) + " ";
    }
    return modtext;
}

std::string tname::relic_charges( item const &it, unsigned int /* quantity */,
                                  segment_bitset const &/* segments */ )
{
    if( it.is_relic() && it.relic_data->max_charges() > 0 && it.relic_data->charges_per_use() > 0 ) {
        return string_format( " (%d/%d)", it.relic_data->charges(), it.relic_data->max_charges() );
    }
    return {};
}

std::string tname::category( item const &it, unsigned int /* quantity */,
                             segment_bitset const &segments )
{
    nc_color const &color = segments[tname::segments::FOOD_PERISHABLE] && it.is_food()
                            ? it.color_in_inventory( &get_avatar() )
                            : c_magenta;
    return colorize( it.get_category_of_contents().name(), color );
}
