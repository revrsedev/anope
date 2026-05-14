#include "module.h"

class AskVersion : public Module
{
 public:
	AskVersion(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD)
	{
		if (Anope::VersionMajor() != 2 || Anope::VersionMinor() != 1)
			throw ModuleException("Requires version 2.1.x of Anope.");

		this->SetAuthor("reverse");
		this->SetVersion("1.1");
	}

	void OnUserConnect(User *user, bool &) override
	{
		if (user->Quitting() || !user->server || !user->server->IsSynced())
			return;

		auto &conf = Config->GetModule(this);
		if (!conf.Get<bool>("enabled", true))
			return;

		const Anope::string &announcer = conf.Get<const Anope::string>("global_announcer", "Global");
		BotInfo *bi = Config->GetClient(announcer);
		if (!bi)
			return;

		user->Extend<time_t>("ask_version_pending", Anope::CurTime);
		Anope::map<Anope::string> tags;
		IRCD->SendPrivmsg(bi, user->GetUID(), "\1VERSION\1", tags);
	}

	void OnBotNotice(User *user, BotInfo *bi, Anope::string &message, const Anope::map<Anope::string> &) override
	{
		Anope::string ctcpname, ctcpbody;
		if (!Anope::ParseCTCP(message, ctcpname, ctcpbody) || !ctcpname.equals_ci("VERSION"))
			return;

		auto *sent = user->GetExt<time_t>("ask_version_pending");
		if (!sent)
			return;

		auto &conf = Config->GetModule(this);
		const auto timeout = conf.Get<time_t>("timeout", "1m");
		if (timeout && Anope::CurTime - *sent > timeout)
		{
			user->Shrink<time_t>("ask_version_pending");
			return;
		}

		user->Shrink<time_t>("ask_version_pending");
		Anope::string versionstr = ctcpbody;
		versionstr.trim();
		if (versionstr.empty())
			versionstr = "(empty)";
		Log(LOG_NORMAL, "ask_version/reply", bi) << "VERSION reply for " << user->nick << ": " << versionstr;
	}
};

MODULE_INIT(AskVersion)