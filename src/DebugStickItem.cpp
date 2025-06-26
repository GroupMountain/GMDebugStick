#include <gmlib/gm/ui/CustomForm.h>
#include <gmlib/mc/locale/I18nAPI.h>
#include <gmlib/mod/item/CustomItemRegistry.h>
#include <gmlib/mod/item/base/ICustomItem.h>
#include <ll/api/base/Containers.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/utils/StringUtils.h>
#include <magic_enum.hpp>
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/legacy/facing/Name.h>
#include <mc/legacy/facing/Rotation.h>
#include <mc/util/BidirectionalUnorderedMap.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/gamemode/InteractionResult.h>
#include <mc/world/item/VanillaItemNames.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/TrialSpawner.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/CandleCount.h>
#include <mc/world/level/block/ComposterBlock.h>
#include <mc/world/level/block/CoralDirection.h>
#include <mc/world/level/block/RailDirection.h>
#include <mc/world/level/block/SculkSensorPhase.h>
#include <mc/world/level/block/WeirdoDirection.h>
#include <mc/world/level/block/states/vanilla_states/VanillaStates.h>
#include <mc/world/level/levelgen/structure/SensibleDirections.h>
#include <string>

namespace DebugStick {

class DebugStickItem : public gmlib::mod::ICustomItem {
public:
    enum class BlockUpdateFlag : uchar {
        None                = 0,
        Neighbors           = 1 << 0,
        Network             = 1 << 1,
        NoGraphic           = 1 << 2,
        Priority            = 1 << 3,
        ForceNoticeListener = 1 << 4,
        All                 = Neighbors | Network, // default value
        AllPriority         = All | Priority,
    };

public:
    DebugStickItem() : ICustomItem(VanillaItemNames::DebugStick()) {}

    uint8_t getItemMaxStackSize() const override { return 1; }

    gmlib::mod::ItemIcon getIcon() const override { return "stick"; }

    std::string getDisplayName() const override { return "item.debug_stick.name"; }

    bool isFoil() const override { return true; }

    Rarity getBaseRarity() const override { return Rarity::Epic; }

    InteractionResult _useOn(ItemStack&, Actor& entity, BlockPos pos, uchar, Vec3 const&) const override {
        static ll::DenseMap<Player*, std::shared_ptr<ll::data::CancellableCallback>> mTimer;
        if (!entity.isPlayer()) return {InteractionResult::Result::Fail};
        auto& player = static_cast<Player&>(entity);
        if (auto it = mTimer.find(&player); it != mTimer.end() && it->second) {
            it->second->cancel();
        }
        mTimer[&player] = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [&player, pos]() -> void {
                static ll::DenseMap<Player*, std::pair<BlockPos, Block const*>> mEditBlocks;
                static auto                                                     mStatesHandler = []() {
                    ll::DenseMap<BlockState const*, std::function<void(gmlib::ui::CustomForm&, Block const&)>> states;

                    auto handlerFunc = [&]<typename T>(T const& state) {
                        using Type = std::remove_cvref_t<typename T::Type>;

                        if constexpr (std::is_same_v<Type, bool>) {
                            states[&state] = [&state](gmlib::ui::CustomForm& form, Block const& block) -> void {
                                auto oldValue = *block.getState<Type>(state);
                                form.appendToggle(
                                    fmt::format("state.{0}.name", state.mName->getString()),
                                    oldValue,
                                    [oldValue, &state](Player& player, bool newValue) -> void {
                                        if (oldValue == newValue) return;
                                        if (auto block = mEditBlocks[&player].second->setState(state, newValue);
                                            block) {
                                            mEditBlocks[&player].second = block.as_ptr();
                                        }
                                    }
                                );
                            };
                        } else if constexpr (std::is_integral_v<Type>) {
                            states[&state] = [&state](gmlib::ui::CustomForm& form, Block const& block) -> void {
                                auto oldValue = std::to_string(*block.getState<Type>(state));
                                form.appendInput(
                                    fmt::format("state.{0}.name", state.mName->getString()),
                                    {},
                                    oldValue,
                                    [oldValue, &state](Player& player, std::string const& newValue) -> void {
                                        if (oldValue == newValue) return;
                                        if (auto result = ll::string_utils::svtonum<Type>(newValue, nullptr, 10);
                                            result) {
                                            if (auto block = mEditBlocks[&player].second->setState(state, *result);
                                                block) {
                                                mEditBlocks[&player].second = block.as_ptr();
                                            }
                                        }
                                    }
                                );
                            };
                        } else if constexpr (std::is_enum_v<Type>) {
                            states[&state] = [&state](gmlib::ui::CustomForm& form, Block const& block) -> void {
                                // clang-format off
                                static auto enumMap = magic_enum::enum_values<Type>() | std::ranges::to<std::vector>();
                                static auto names   = 
                                    enumMap
                                    | std::views::transform([](auto sv) {
                                            return fmt::format(
                                                "%enum.{0}.{1}",
                                                ll::reflection::type_stem_name_v<Type>,
                                                magic_enum::enum_name(sv)
                                            );
                                        })
                                    | std::ranges::to<std::vector>();
                                    
                                auto oldValue = std::find(
                                    enumMap.begin(),
                                    enumMap.end(),
                                    static_cast<Type>(*block.getState<int>(state))
                                ) - enumMap.begin();
                                // clang-format on

                                form.appendDropdown(
                                    fmt::format("state.{0}.name", state.mName->getString()),
                                    names,
                                    oldValue,
                                    [oldValue, &state](Player& player, int64 value) -> void {
                                        if (oldValue == value) return;
                                        // clang-format off
                                        if (
                                            auto block = mEditBlocks[&player].second->setState(
                                                state,
                                                static_cast<int>(enumMap.at(value))
                                            ); block
                                        ) {
                                            mEditBlocks[&player].second = block.as_ptr();
                                        }
                                        // clang-format on
                                    }
                                );
                            };
                        } else {
                            static_assert(ll::traits::always_false<T>, "Unsupported state type");
                        }
                    };

                    {
                        handlerFunc(VanillaStates::Active());
                        handlerFunc(VanillaStates::Age());
                        handlerFunc(VanillaStates::AgeBit());
                        handlerFunc(VanillaStates::AttachedBit());
                        handlerFunc(VanillaStates::Attachment());
                        handlerFunc(VanillaStates::BambooLeafSize());
                        handlerFunc(VanillaStates::BambooThickness());
                        handlerFunc(VanillaStates::BeehiveHoneyLevel());
                        handlerFunc(VanillaStates::BigDripleafHead());
                        handlerFunc(VanillaStates::BigDripleafTilt());
                        handlerFunc(VanillaStates::BiteCounter());
                        handlerFunc(VanillaStates::Bloom());
                        handlerFunc(VanillaStates::BookshelfOccupiedSlots());
                        handlerFunc(VanillaStates::BrewingStandSlotABit());
                        handlerFunc(VanillaStates::BrewingStandSlotBBit());
                        handlerFunc(VanillaStates::BrewingStandSlotCBit());
                        handlerFunc(VanillaStates::BrushedProgress());
                        handlerFunc(VanillaStates::ButtonPressedBit());
                        handlerFunc(VanillaStates::CanSummon());
                        handlerFunc(reinterpret_cast<BlockStateVariant<CandleCount> const&>(VanillaStates::Candles()));
                        handlerFunc(VanillaStates::CauldronLiquid());
                        handlerFunc(VanillaStates::ClusterCount());
                        handlerFunc(VanillaStates::ComposterFillLevel());
                        handlerFunc(VanillaStates::ConditionalBit());
                        handlerFunc(
                            reinterpret_cast<BlockStateVariant<CoralDirection> const&>(VanillaStates::CoralDirection())
                        );
                        handlerFunc(VanillaStates::CoralFanDirection());
                        handlerFunc(VanillaStates::CoveredBit());
                        handlerFunc(VanillaStates::CrackedState());
                        handlerFunc(VanillaStates::Crafting());
                        handlerFunc(VanillaStates::CreakingHeartState());
                        handlerFunc(VanillaStates::DEPRECATED());
                        handlerFunc(VanillaStates::DEPRECATED_AllowUnderwaterBit());
                        handlerFunc(VanillaStates::DEPRECATED_BlockLightLevel());
                        handlerFunc(VanillaStates::DEPRECATED_ChemistryTableType());
                        handlerFunc(VanillaStates::DEPRECATED_ChiselType());
                        handlerFunc(VanillaStates::DEPRECATED_Color());
                        handlerFunc(VanillaStates::DEPRECATED_ColorBit());
                        handlerFunc(VanillaStates::DEPRECATED_CoralColor());
                        handlerFunc(VanillaStates::DEPRECATED_CoralHangTypeBit());
                        handlerFunc(VanillaStates::DEPRECATED_Damage());
                        handlerFunc(VanillaStates::DEPRECATED_DirtType());
                        handlerFunc(VanillaStates::DEPRECATED_DoublePlantType());
                        handlerFunc(VanillaStates::DEPRECATED_FlowerType());
                        handlerFunc(VanillaStates::DEPRECATED_MonsterEggStoneType());
                        handlerFunc(VanillaStates::DEPRECATED_NewLeavesType());
                        handlerFunc(VanillaStates::DEPRECATED_NewLogType());
                        handlerFunc(VanillaStates::DEPRECATED_NoDropBit());
                        handlerFunc(VanillaStates::DEPRECATED_OldLeavesType());
                        handlerFunc(VanillaStates::DEPRECATED_OldLogType());
                        handlerFunc(VanillaStates::DEPRECATED_PrismarineBlockType());
                        handlerFunc(VanillaStates::DEPRECATED_SandType());
                        handlerFunc(VanillaStates::DEPRECATED_SandstoneType());
                        handlerFunc(VanillaStates::DEPRECATED_SaplingType());
                        handlerFunc(VanillaStates::DEPRECATED_SpongeType());
                        handlerFunc(VanillaStates::DEPRECATED_StoneBrickType());
                        handlerFunc(VanillaStates::DEPRECATED_StoneSlabType());
                        handlerFunc(VanillaStates::DEPRECATED_StoneSlabType2());
                        handlerFunc(VanillaStates::DEPRECATED_StoneSlabType3());
                        handlerFunc(VanillaStates::DEPRECATED_StoneSlabType4());
                        handlerFunc(VanillaStates::DEPRECATED_StoneType());
                        handlerFunc(VanillaStates::DEPRECATED_StrippedBit());
                        handlerFunc(VanillaStates::DEPRECATED_StructureVoidType());
                        handlerFunc(VanillaStates::DEPRECATED_TallGrassType());
                        handlerFunc(VanillaStates::DEPRECATED_WallBlockType());
                        handlerFunc(VanillaStates::DEPRECATED_WoodType());
                        handlerFunc(VanillaStates::DeadBit());
                        handlerFunc(
                            reinterpret_cast<BlockStateVariant<SensibleDirections> const&>(VanillaStates::Direction())
                        );
                        handlerFunc(VanillaStates::DisarmedBit());
                        handlerFunc(VanillaStates::DoorHingeBit());
                        handlerFunc(VanillaStates::DragDown());
                        handlerFunc(VanillaStates::DripstoneThickness());
                        handlerFunc(VanillaStates::EndPortalEyeBit());
                        handlerFunc(VanillaStates::ExplodeBit());
                        handlerFunc(VanillaStates::Extinguished());
                        handlerFunc(
                            reinterpret_cast<BlockStateVariant<Facing::Name> const&>(VanillaStates::FacingDirection())
                        );
                        handlerFunc(VanillaStates::FillLevel());
                        handlerFunc(VanillaStates::GrowingPlantAge());
                        handlerFunc(VanillaStates::Growth());
                        handlerFunc(VanillaStates::HangingBit());
                        handlerFunc(VanillaStates::HeadPieceBit());
                        handlerFunc(VanillaStates::Height());
                        handlerFunc(VanillaStates::HugeMushroomBits());
                        handlerFunc(VanillaStates::InWallBit());
                        handlerFunc(VanillaStates::InfiniburnBit());
                        handlerFunc(VanillaStates::ItemFrameMapBit());
                        handlerFunc(VanillaStates::ItemFramePhotoBit());
                        handlerFunc(VanillaStates::KelpAge());
                        handlerFunc(VanillaStates::LeverDirection());
                        handlerFunc(VanillaStates::LiquidDepth());
                        handlerFunc(VanillaStates::Lit());
                        handlerFunc(VanillaStates::MoisturizedAmount());
                        handlerFunc(VanillaStates::MultiFaceDirectionBits());
                        handlerFunc(VanillaStates::Natural());
                        handlerFunc(VanillaStates::OccupiedBit());
                        handlerFunc(VanillaStates::Ominous());
                        handlerFunc(VanillaStates::OpenBit());
                        handlerFunc(VanillaStates::Orientation());
                        handlerFunc(VanillaStates::OutputLitBit());
                        handlerFunc(VanillaStates::OutputSubtractBit());
                        handlerFunc(VanillaStates::PaleMossCarpetSideEast());
                        handlerFunc(VanillaStates::PaleMossCarpetSideNorth());
                        handlerFunc(VanillaStates::PaleMossCarpetSideSouth());
                        handlerFunc(VanillaStates::PaleMossCarpetSideWest());
                        handlerFunc(VanillaStates::PersistentBit());
                        handlerFunc(VanillaStates::PillarAxis());
                        handlerFunc(VanillaStates::PortalAxis());
                        handlerFunc(VanillaStates::PoweredBit());
                        handlerFunc(VanillaStates::PropaguleStage());
                        handlerFunc(VanillaStates::RailDataBit());
                        handlerFunc(
                            reinterpret_cast<BlockStateVariant<RailDirection> const&>(VanillaStates::RailDirection())
                        );
                        handlerFunc(VanillaStates::RedstoneSignal());
                        handlerFunc(VanillaStates::RepeaterDelay());
                        handlerFunc(VanillaStates::RespawnAnchorCharge());
                        handlerFunc(
                            reinterpret_cast<BlockStateVariant<Facing::Rotation> const&>(VanillaStates::Rotation())
                        );
                        handlerFunc(reinterpret_cast<BlockStateVariant<SculkSensorPhase> const&>(
                            VanillaStates::SculkSensorPhase()
                        ));
                        handlerFunc(VanillaStates::SeagrassType());
                        handlerFunc(VanillaStates::Stability());
                        handlerFunc(VanillaStates::StabilityCheckBit());
                        handlerFunc(VanillaStates::StandingRotation());
                        handlerFunc(VanillaStates::StructureBlockType());
                        handlerFunc(VanillaStates::SuspendedBit());
                        handlerFunc(VanillaStates::Tip());
                        handlerFunc(VanillaStates::ToggleBit());
                        handlerFunc(VanillaStates::TopSlotBit());
                        handlerFunc(VanillaStates::TorchFacingDirection());
                        handlerFunc(reinterpret_cast<BlockStateVariant<TrialSpawner::SpawningLogicState> const&>(
                            VanillaStates::TrialSpawnerState()
                        ));
                        handlerFunc(VanillaStates::TriggeredBit());
                        handlerFunc(VanillaStates::TurtleEggCount());
                        handlerFunc(VanillaStates::TwistingVinesAge());
                        handlerFunc(VanillaStates::UpdateBit());
                        handlerFunc(VanillaStates::UpperBlockBit());
                        handlerFunc(VanillaStates::UpsideDownBit());
                        handlerFunc(VanillaStates::VaultState());
                        handlerFunc(VanillaStates::VineDirectionBits());
                        handlerFunc(VanillaStates::WallConnectionTypeEast());
                        handlerFunc(VanillaStates::WallConnectionTypeNorth());
                        handlerFunc(VanillaStates::WallConnectionTypeSouth());
                        handlerFunc(VanillaStates::WallConnectionTypeWest());
                        handlerFunc(VanillaStates::WallPostBit());
                        handlerFunc(VanillaStates::WeepingVinesAge());
                        handlerFunc(reinterpret_cast<BlockStateVariant<WeirdoDirection> const&>(
                            VanillaStates::WeirdoDirection()
                        ));
                    }
                    return states;
                }();

                auto* block = (mEditBlocks[&player] = {pos, &player.getDimensionBlockSource().getBlock(pos)}).second;
                auto  title = fmt::format("%{0}.name", block->mLegacyBlock->mDescriptionId.get());
                if (gmlib::I18nAPI::get(title) == title.substr(1)) {
                    title = fmt::format(
                        "%{0}.name",
                        block->mLegacyBlock->asItemInstance(*block, nullptr).mItem->getDescriptionId()
                    );
                }
                gmlib::ui::CustomForm form(title);
                bool showStates = false;
                for (auto& [state, func] : mStatesHandler) {
                    if (!block->hasState(*state)) continue;
                    func(form, *block);
                    showStates = true;
                }
                if (!showStates) return;
                form.sendTo(player, [](Player& player, auto&, std::optional<ModalFormCancelReason> reason) -> void {
                    if (reason) return;
                    if (auto it = mEditBlocks.find(&player); it != mEditBlocks.end()) {
                        player.getDimensionBlockSource().setBlock(
                            it->second.first,
                            *it->second.second,
                            static_cast<int>(BlockUpdateFlag::Network),
                            nullptr,
                            nullptr
                        );
                        mEditBlocks.erase(it);
                    }
                });
            },
            std::chrono::milliseconds(50)
        );
        return {InteractionResult::Result::Swing};
    }
};

} // namespace DebugStick

inline static auto GMLIB_CUSTOM_ITEM_DebugStickItem = []() -> bool {
    gmlib::mod::CustomItemRegistry::getInstance().registerItem<DebugStick::DebugStickItem>();
    return true;
}();