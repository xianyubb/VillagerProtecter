#include "ll/api/Logger.h"
#include "mod/Config.h"
#include "mod/MyMod.h"

#include "ll/api/Config.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/SpawnMobEvent.h"
#include "ll/api/form/CustomForm.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"


#include "mc/entity/utilities/ActorType.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/scores/Objective.h"
#include "mc/world/scores/ScoreInfo.h"
#include "mc/world/scores/Scoreboard.h"


#include <string>


void ModInit() {
    auto&       self           = my_mod::MyMod::getInstance().getSelf();
    auto&       logger         = self.getLogger();
    const auto& configFilePath = self.getConfigDir() / "config.json";
    if (!ll::config::loadConfig(config, configFilePath)) {
        logger.warn("加载配置文件失败: {}", configFilePath);
        logger.info("将生成默认配置文件...");

        if (!ll::config::saveConfig(config, configFilePath)) {
            logger.error("生成默认配置文件失败: {}", configFilePath);
        }
    }
};


void ReloadConfig() {
    auto&       self           = my_mod::MyMod::getInstance().getSelf();
    const auto& configFilePath = self.getConfigDir() / "config.json";
    if (!ll::config::loadConfig(config, configFilePath)) {
        self.getLogger().error("重新加载配置文件失败: {}", configFilePath);
        return;
    }
}

void RegReloadCommand() {
    auto& cmdBus = ll::command::CommandRegistrar::getInstance();
    auto& cmd    = cmdBus.getOrCreateCommand("vp", "重载", CommandPermissionLevel::GameDirectors);
    cmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (origin.getOriginType() == CommandOriginType::DedicatedServer) {
            ReloadConfig();
            output.success("[村民保护着] 重载配置文件成功");
            return;
        };
        auto entity = origin.getEntity();
        if (entity == nullptr || !entity->isType(ActorType::Player)) {
            output.error("[村民保护着] 此命令只能玩家或者控制台使用");
            return;
        }
        ReloadConfig();
        output.success("[村民保护着] 重载配置文件成功");
    });
}

void SendForm(Player& player, Actor& mob) {
    ll::form::SimpleForm Form;
    Form.setTitle("村民保护者")
        .appendButton(
            "开启村民无敌",
            [&](Player& pl) {
                if (mob.hasTag("我无敌了")) {
                    pl.sendMessage("[村民保护者] 该村民已经无敌!!!");
                    return;
                }
                Scoreboard& scoreboard = ll::service::getLevel()->getScoreboard();
                Objective*  objective  = scoreboard.getObjective(config.ScoreName);
                if (objective == nullptr) {
                    my_mod::MyMod::getInstance().getSelf().getLogger().error(
                        "Objective {} not found",
                        config.ScoreName
                    );
                    return;
                };
                const ScoreboardId& id = scoreboard.getScoreboardId(player);
                if (!id.isValid()) {
                    scoreboard.createScoreboardId(player);
                }
                auto score = objective->getPlayerScore(id).mScore;
                if (score < config.cast) {
                    player.sendMessage("[村民保护者] 您的积分不够");
                    return;
                }
                score    -= config.cast;
                bool res  = false;
                scoreboard.modifyPlayerScore(res, id, *objective, score, PlayerScoreSetFunction::Set);
                if (!res) {
                    my_mod::MyMod::getInstance().getSelf().getLogger().error("[村民保护者] 修改积分失败");
                    return;
                };
                mob.addTag("我无敌了");
                pl.sendMessage("[村民保护者] 开启成功");
            }
        )
        .appendButton(
            "关闭村民无敌",
            [&](Player& pl) {
                if (!mob.hasTag("我无敌了")) {
                    pl.sendMessage("[村民保护者] 该村民并未无敌!!!");
                    return;
                }
                mob.removeTag("我无敌了");
                pl.sendMessage("[村民保护者] 关闭成功");
            }
        )
        .appendButton(
            "修改命名",
            [&](Player& pl) {
                ll::form::CustomForm Form_2;
                Form_2.setTitle("村民保护者")
                    .appendInput("input", "请输入新命名: ", "string", mob.getNameTag())
                    .sendTo(
                        pl,
                        [&](Player&                           pl_,
                            ll::form::CustomFormResult const& fm,
                            std::optional<ModalFormCancelReason>) -> void {
                            if (!fm.has_value()) {
                                return;
                            }
                            mob.setNameTag(std::get<std::string>(fm->at("input")));
                            pl_.sendMessage("[村民保护者] 命名修改成功");
                        }
                    );
            }
        )
        .sendTo(player);
}


LL_AUTO_TYPE_INSTANCE_HOOK(
    MobHurtEffectHook,
    HookPriority::Low,
    Mob,
    &Mob::getDamageAfterResistanceEffect,
    float,
    ActorDamageSource const& source,
    float                    damage
) {
    if (source.isEntitySource()) {
        if (source.getEntityType() == ActorType::Zombie || source.getEntityType() == ActorType::ZombieVillager
            || source.getEntityType() == ActorType::ZombieVillagerV2) {
            return origin(source, damage);
        }
    }
    if ((this->isType(ActorType::Villager) || this->isType(ActorType::VillagerV2)
         || this->isType(ActorType::VillagerBase))
        && this->hasTag("我无敌了")) {
        return false;
    }
    return origin(source, damage);
}


void RegListener() {
    auto& bus = ll::event::EventBus::getInstance();
    bus.emplaceListener<ll::event::SpawnedMobEvent>([](ll::event::SpawnedMobEvent& ev) {
        auto mob = ev.mob();
        if (mob.has_value()) {
            if (mob->isType(ActorType::Villager) || mob->isType(ActorType::VillagerV2)
                || mob->isType(ActorType::VillagerBase)) {
                mob->setNameTag("Villager");
            }
        }
    });
}

LL_AUTO_TYPE_INSTANCE_HOOK(
    PlayerAttackEventHook,
    HookPriority::Normal,
    Player,
    "?attack@Player@@UEAA_NAEAVActor@@AEBW4ActorDamageCause@@@Z",
    bool,
    Actor&                  actor,
    ActorDamageCause const& cause
) {
    if ((actor.isType(ActorType::Villager) || actor.isType(ActorType::VillagerV2)
         || actor.isType(ActorType::VillagerBase))
        && this->getSelectedItem().getTypeName() == "" && this->isSneaking() == true) {
        SendForm(*this, actor);
        return false;
    }
    return origin(actor, cause);
}
