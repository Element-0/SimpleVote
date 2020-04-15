#include <Command/Command.h>
#include <Command/CommandOrigin.h>
#include <Command/CommandOutput.h>
#include <Command/CommandRegistry.h>
#include <Actor/ServerPlayer.h>

#include <dllentry.h>
#include <exception>
#include <log.h>
#include <command.h>
#include <playerdb.h>

#include "settings.h"

DEF_LOGGER("SimpleVote");
DEFAULT_SETTINGS(settings);

void dllenter() {}
void dllexit() {}

void BeforeUnload() {
  OnExit();
}

class VoteCommand : public Command {
public:
  void execute(const CommandOrigin &orig, CommandOutput &outp) override {
    AddTask([] {
      try {
        auto str = Fetch(L"/v2/top/10");
        LOGV("ret: %s") % str;
      } catch (std::exception const &ex) { LOGE("err: %s") % ex.what(); }
    });
    outp.success();
  }

  static void setup(CommandRegistry *registry) {
    using namespace commands;
    registry->registerCommand(
        "vote", "commands.vote.description", CommandPermissionLevel::GameMasters, CommandFlagCheat, CommandFlagNone);
    registry->registerOverload<VoteCommand>("vote");
  }
};

void PreInit() {
  InitHttp();
  Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), VoteCommand::setup);
}