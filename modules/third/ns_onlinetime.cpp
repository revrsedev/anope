/*
2024 Jean "reverse" Chevronnet
Module for Anope IRC Services v2.1.
Tracks and displays the total online time for NickServ accounts.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "module.h"

class NSOnlineTime final
	: public Module
{
	SerializableExtensibleItem<uint64_t> total_seconds;
	PrimitiveExtensibleItem<unsigned> active_sessions;
	PrimitiveExtensibleItem<time_t> session_start;
	PrimitiveExtensibleItem<Anope::string> tracked_account;

	Anope::string FormatTime(uint64_t seconds)
	{
		const uint64_t years = seconds / 31536000;
		seconds %= 31536000;

		const uint64_t months = seconds / 2592000;
		seconds %= 2592000;

		const uint64_t days = seconds / 86400;
		seconds %= 86400;

		const uint64_t hours = seconds / 3600;
		const uint64_t minutes = (seconds % 3600) / 60;

		std::vector<Anope::string> parts;
		if (years)
			parts.push_back(Anope::ToString(years) + (years == 1 ? " year" : " years"));
		if (months)
			parts.push_back(Anope::ToString(months) + (months == 1 ? " month" : " months"));
		if (days)
			parts.push_back(Anope::ToString(days) + (days == 1 ? " day" : " days"));
		if (hours)
			parts.push_back(Anope::ToString(hours) + (hours == 1 ? " hour" : " hours"));
		if (minutes)
			parts.push_back(Anope::ToString(minutes) + (minutes == 1 ? " minute" : " minutes"));

		if (parts.empty())
			return "0 minutes";

		Anope::string out;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (i)
				out += ", ";
			out += parts[i];
		}

		return out;
	}

	void StartTracking(User *u)
	{
		if (!u || !u->Account() || tracked_account.HasExt(u))
			return;

		NickCore *nc = u->Account();
		tracked_account.Set(u, nc->display);

		unsigned *active = active_sessions.Get(nc);
		if (!active)
			active = nc->Extend<unsigned>("ns_onlinetime_active");

		if (*active == 0)
			session_start.Set(nc, Anope::CurTime);

		++(*active);
	}

	void StopTracking(User *u)
	{
		if (!u)
			return;

		Anope::string *account_name = tracked_account.Get(u);
		if (!account_name)
			return;

		NickCore *nc = NickCore::Find(*account_name);
		tracked_account.Unset(u);

		if (!nc)
			return;

		unsigned *active = active_sessions.Get(nc);
		if (!active || *active == 0)
			return;

		--(*active);
		if (*active != 0)
			return;

		time_t *start = session_start.Get(nc);
		if (start && *start > 0 && Anope::CurTime > *start)
		{
			uint64_t *total = total_seconds.Get(nc);
			if (!total)
				total = nc->Extend<uint64_t>("ns_onlinetime_total");
			*total += static_cast<uint64_t>(Anope::CurTime - *start);
		}

		session_start.Set(nc, 0);
	}

	uint64_t GetCurrentTotal(NickCore *nc)
	{
		uint64_t total = 0;
		if (uint64_t *stored = total_seconds.Get(nc))
			total = *stored;

		unsigned *active = active_sessions.Get(nc);
		time_t *start = session_start.Get(nc);
		if (active && *active > 0 && start && *start > 0 && Anope::CurTime > *start)
			total += static_cast<uint64_t>(Anope::CurTime - *start);

		return total;
	}

 public:
	NSOnlineTime(const Anope::string &modname, const Anope::string &creator)
		: Module(modname, creator, THIRD)
		, total_seconds(this, "ns_onlinetime_total")
		, active_sessions(this, "ns_onlinetime_active")
		, session_start(this, "ns_onlinetime_start")
		, tracked_account(this, "ns_onlinetime_user_account")
	{
		if (Anope::VersionMajor() != 2 || Anope::VersionMinor() < 1)
			throw ModuleException("Requires version 2.1.x (or newer) of Anope.");

		this->SetAuthor("reverse");
		this->SetVersion("2.0.0");
	}

	~NSOnlineTime()
	{
		for (const auto &[_, u] : UserListByNick)
			StopTracking(u);
	}

	void OnUplinkSync(Server *) override
	{
		for (const auto &[_, u] : UserListByNick)
			StartTracking(u);
	}

	void OnUserLogin(User *u) override
	{
		StartTracking(u);
	}

	void OnNickLogout(User *u) override
	{
		StopTracking(u);
	}

	void OnUserQuit(User *u, const Anope::string &) override
	{
		StopTracking(u);
	}

	void OnNickInfo(CommandSource &, NickAlias *na, InfoFormatter &info, bool) override
	{
		info["Total time online"] = FormatTime(GetCurrentTotal(na->nc));
	}
};

MODULE_INIT(NSOnlineTime)