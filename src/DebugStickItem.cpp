#include <gmlib/mod/item/CustomItemRegistry.h>
#include <gmlib/mod/item/base/ICustomItem.h>
#include <ll/api/base/Containers.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/gamemode/InteractionResult.h>
#include <mc/world/item/VanillaItemNames.h>
#include <mc/world/level/BlockPos.h>

namespace DebugStick {

class DebugStickItem : public gmlib::mod::ICustomItem {
public:
    DebugStickItem() : ICustomItem(VanillaItemNames::DebugStick()){}

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
            [&player, pos]() -> void { //
                auto& region = player.getDimensionBlockSource();
                auto& oldBlock  = region.getBlock(pos);
                auto* newBlock  = oldBlock.getLegacyBlock().getNextBlockPermutation(oldBlock);
                if (newBlock) region.setBlock(pos, *newBlock, 3, nullptr, &player);
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