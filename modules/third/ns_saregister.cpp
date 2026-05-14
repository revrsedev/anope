/* Original Module Author: Adam
 * Updated for 1.9.4 (and beyond?): Laura
 * Date of original: 14/02/10
 *
 * Updated for 2.0.x: Gottem
 * Date: 2019/04/25
 *
 * Updated for 2.1.x: Gottem
 * Date: 2023/11/27
 **********************************************
 * Module used in: Nickserv
 * Syntax: /ns saregister 'nick' 'pass' 'email'
 **********************************************
*/
#include "module.h"

#define AUTHOR "Gottem"
#define VERSION "3.2.0"

#define INVALID_NICKNAME "This nickname is invalid for use with standard RFC 2812"

class CommandNSSaRegister : public Command {
 public:
	CommandNSSaRegister(Module *creator) : Command(creator, "nickserv/saregister", 3, 3) { }

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override {
		const Anope::string &nick = params[0];
		const Anope::string &password = params[1];
		const Anope::string &email = params[2];
		const Anope::string &nsregister = Config->GetModule("nickserv").Get<const Anope::string>("registration");
		size_t nicklen = nick.length();
		unsigned int passlen = Config->GetModule("nickserv").Get<unsigned>("passlen", "32");

		if(Anope::ReadOnly) {
			source.Reply(_("Sorry, nickname registration is temporarily disabled."));
			return;
		}

		if(nsregister.equals_ci("disable")) {
			source.Reply(_("Registration is currently disabled."));
			return;
		}

		if(!IRCD->IsNickValid(nick)) {
			source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
			return;
		}

		if(BotInfo::Find(nick, true)) {
			source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
			return;
		}

		if(isdigit(nick[0]) || nick[0] == '-') {
			source.Reply(INVALID_NICKNAME);
			return;
		}

		const Anope::string &guestnick = Config->GetModule("nickserv").Get<const Anope::string>("guestnickprefix", "Guest");
		if(nicklen <= guestnick.length() + 7 && nicklen >= guestnick.length() + 1 && !nick.find_ci(guestnick) && nick.substr(guestnick.length()).find_first_not_of("1234567890") == Anope::string::npos) {
			source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
			return;
		}

		if(Config->GetModule("nickserv").Get<bool>("restrictopernicks")) {
			for(unsigned i = 0; i < Oper::opers.size(); ++i) {
				Oper *o = Oper::opers[i];
				if(!source.IsOper() && nick.find_ci(o->name) != Anope::string::npos) {
					source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
					return;
				}
			}
		}

		if(NickAlias::Find(nick) != NULL)
			source.Reply(NICK_ALREADY_REGISTERED, nick.c_str());
		else if(password.equals_ci(nick) || (Config->GetBlock("options").Get<bool>("strictpasswords") && password.length() < 5))
			source.Reply(_("Password is not complex enough. Please choose a more obscure password."));
		else if(password.length() > passlen)
			source.Reply(_("Your password is too long. Please try again with a shorter password, no longer than %u characters."), passlen);
		else if(!email.empty() && !Mail::Validate(email))
			source.Reply(MAIL_X_INVALID, email.c_str());
		else {
			NickCore *nc = new NickCore(nick);
			NickAlias *na = new NickAlias(nick, nc);
			User *u = User::Find(nick, true);
			Anope::Encrypt(password, nc->pass);
			if(!email.empty())
				nc->email = email;

			if(u)
				na->last_userhost = u->GetIdent() + "@" + u->GetDisplayedHost();
			else
				na->last_userhost = "*@*";

			Log(LOG_ADMIN, source, this) << "to register " << nick << " (email: " << email << ")";
			source.Reply(_("Your forced registration for nick \002%s\002 succeeded (password = \002%s\002)."), na->nick.c_str(), password.c_str());

			if(u) {
				u->Identify(na);
				u->lastnickreg = Anope::CurTime;
			}
		}
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) override {
		source.Reply(_("Syntax: \2SAREGISTER \37nick\37 \37password\37 \37email\37"));
		source.Reply(_("Allows services admins to register other nicks"));
		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand) override {
		source.Reply(_("Syntax: \2SAREGISTER \37nick\37 \37password\37 \37email\37"));
	}
};

class NSSaRegister : public Module {
	CommandNSSaRegister commandnssaregister;

 public:
	NSSaRegister(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD), commandnssaregister(this) {
		this->SetAuthor(AUTHOR);
		this->SetVersion(VERSION);
	}
};

MODULE_INIT(NSSaRegister)
