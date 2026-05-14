/*
2024 Jean "reverse" Chevronnet
Module for Anope IRC Services v2.1.
Botserv feed URL storage with optional announce.

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

/// $LinkerFlags: -lcurl -lssl -lcrypto

#include "module.h"
#include "timers.h"

#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#define FEEDS_CHANNEL_DATA_TYPE "FeedsChannel"
#define FEEDS_STATE_DATA_TYPE "FeedsState"

struct FeedChannelData;
struct FeedsState;

using feed_channel_map = Anope::unordered_map<FeedChannelData *>;
static Serialize::Checker<feed_channel_map> FeedChannelList(FEEDS_CHANNEL_DATA_TYPE);

using feeds_state_map = Anope::unordered_map<FeedsState *>;
static Serialize::Checker<feeds_state_map> FeedsStateList(FEEDS_STATE_DATA_TYPE);

struct FeedEntry final
{
	Anope::string url;
	Anope::string title;
	Anope::string last_item_id;
	Anope::string last_item_url;
	Anope::string last_item_title;
	Anope::string added_by;
	time_t added_at = 0;
	time_t last_seen = 0;
};

struct FeedChannelData final
	: Serializable
{
	Anope::string name;
	time_t updated = 0;
	std::vector<FeedEntry> feeds;

	explicit FeedChannelData(const Anope::string& chname)
		: Serializable(FEEDS_CHANNEL_DATA_TYPE)
		, name(chname)
	{
		if (chname.empty())
			throw ModuleException("Feeds: empty channel name");

		FeedChannelList->insert_or_assign(this->name, this);
	}

	~FeedChannelData() override
	{
		FeedChannelList->erase(this->name);
	}
};

struct FeedsState final
	: Serializable
{
	Anope::string name = "state";
	time_t updated = 0;
	uint64_t total_feeds = 0;

	FeedsState()
		: Serializable(FEEDS_STATE_DATA_TYPE)
	{
		FeedsStateList->insert_or_assign(this->name, this);
	}

	~FeedsState() override
	{
		FeedsStateList->erase(this->name);
	}
};

class FeedsChannelDataType final
	: public Serialize::Type
{
public:
	explicit FeedsChannelDataType(Module* owner)
		: Serialize::Type(FEEDS_CHANNEL_DATA_TYPE, owner)
	{
	}

	void Serialize(Serializable* obj, Serialize::Data& data) const override
	{
		const auto* rec = static_cast<const FeedChannelData*>(obj);
		data.Store("name", rec->name);
		data.Store("updated", static_cast<int64_t>(rec->updated));

		data.Store("feedcount", static_cast<uint64_t>(rec->feeds.size()));
		for (uint64_t i = 0; i < static_cast<uint64_t>(rec->feeds.size()); ++i)
		{
			const auto& f = rec->feeds[static_cast<size_t>(i)];
			const Anope::string prefix = "feed" + Anope::ToString(i) + ".";
			data.Store(prefix + "url", f.url);
			data.Store(prefix + "title", f.title);
			data.Store(prefix + "last_item_id", f.last_item_id);
			data.Store(prefix + "last_item_url", f.last_item_url);
			data.Store(prefix + "last_item_title", f.last_item_title);
			data.Store(prefix + "added_by", f.added_by);
			data.Store(prefix + "added_at", static_cast<int64_t>(f.added_at));
			data.Store(prefix + "last_seen", static_cast<int64_t>(f.last_seen));
		}
	}

	Serializable* Unserialize(Serializable* obj, Serialize::Data& data) const override
	{
		Anope::string name;
		data.TryLoad("name", name);
		if (name.empty())
			return nullptr;

		FeedChannelData* rec = nullptr;
		if (obj)
		{
			rec = anope_dynamic_static_cast<FeedChannelData*>(obj);
		}
		else
		{
			auto it = FeedChannelList->find(name);
			if (it != FeedChannelList->end())
				rec = it->second;
			if (!rec)
				rec = new FeedChannelData(name);
		}

		data.TryLoad("updated", rec->updated);

		uint64_t feedcount = 0;
		data.TryLoad("feedcount", feedcount);
		rec->feeds.clear();
		rec->feeds.reserve(static_cast<size_t>(feedcount));
		for (uint64_t i = 0; i < feedcount; ++i)
		{
			FeedEntry f;
			const Anope::string prefix = "feed" + Anope::ToString(i) + ".";
			data.TryLoad(prefix + "url", f.url);
			data.TryLoad(prefix + "title", f.title);
			data.TryLoad(prefix + "last_item_id", f.last_item_id);
			data.TryLoad(prefix + "last_item_url", f.last_item_url);
			data.TryLoad(prefix + "last_item_title", f.last_item_title);
			data.TryLoad(prefix + "added_by", f.added_by);
			data.TryLoad(prefix + "added_at", f.added_at);
			data.TryLoad(prefix + "last_seen", f.last_seen);
			if (!f.url.empty())
				rec->feeds.push_back(std::move(f));
		}

		FeedChannelList->insert_or_assign(name, rec);
		return rec;
	}
};

class FeedsStateDataType final
	: public Serialize::Type
{
public:
	explicit FeedsStateDataType(Module* owner)
		: Serialize::Type(FEEDS_STATE_DATA_TYPE, owner)
	{
	}

	void Serialize(Serializable* obj, Serialize::Data& data) const override
	{
		const auto* st = static_cast<const FeedsState*>(obj);
		data.Store("name", st->name);
		data.Store("updated", static_cast<int64_t>(st->updated));
		data.Store("total_feeds", static_cast<uint64_t>(st->total_feeds));
	}

	Serializable* Unserialize(Serializable* obj, Serialize::Data& data) const override
	{
		Anope::string name;
		data.TryLoad("name", name);
		if (name.empty())
			return nullptr;

		FeedsState* st = nullptr;
		if (obj)
		{
			st = anope_dynamic_static_cast<FeedsState*>(obj);
		}
		else
		{
			auto it = FeedsStateList->find(name);
			if (it != FeedsStateList->end())
				st = it->second;
			if (!st)
				st = new FeedsState();
		}

		st->name = name;
		data.TryLoad("updated", st->updated);
		data.TryLoad("total_feeds", st->total_feeds);
		FeedsStateList->insert_or_assign(st->name, st);
		return st;
	}
};

class FeedsModule final
	: public Module
{
	static constexpr size_t kMaxFeedBytes = 512 * 1024;

	class DeferredSaveTimer final
		: public Timer
	{
		FeedsModule& fm;

	public:
		DeferredSaveTimer(FeedsModule& owner, time_t seconds)
			: Timer(&owner, seconds)
			, fm(owner)
		{
		}

		bool Tick() override
		{
			if (!this->fm.db_save_pending)
				return false;
			if (!Me || !Me->IsSynced())
				return true;
			this->fm.db_save_pending = false;
			Anope::SaveDatabases();
			return false;
		}
	};

	FeedsChannelDataType channel_type;
	FeedsStateDataType state_type;
	FeedsState* state = nullptr;

	bool db_save_pending = false;
	DeferredSaveTimer* db_save_timer = nullptr;

	Anope::string announce_format;
	time_t poll_interval = 300;
	unsigned int max_per_poll = 1;

	class FeedsPollTimer final
		: public Timer
	{
		FeedsModule& fm;

	public:
		FeedsPollTimer(FeedsModule& owner, time_t seconds)
			: Timer(&owner, seconds)
			, fm(owner)
		{
		}

		bool Tick() override
		{
			this->fm.PollTick();
			return true;
		}
	};

	std::unique_ptr<FeedsPollTimer> poll_timer;

	class CommandBSFeeds final
		: public Command
	{
		FeedsModule& fm;

	public:
		CommandBSFeeds(Module* creator, FeedsModule& module)
			: Command(creator, "botserv/feeds", 2, 3)
			, fm(module)
		{
			this->SetDesc(_("Manage per-channel feed URLs"));
			this->SetSyntax(_("ADD \037channel\037 \037url\037"));
			this->SetSyntax(_("DEL \037channel\037 \037url\037"));
			this->SetSyntax(_("LIST \037channel\037"));
		}

		void Execute(CommandSource& source, const std::vector<Anope::string>& params) override
		{
			const Anope::string& subcommand = params[0];
			const Anope::string& channel = params[1];
			const Anope::string url = (params.size() >= 3) ? params[2] : "";

			ChannelInfo* ci = ChannelInfo::Find(channel);
			if (ci == NULL)
			{
				source.Reply(CHAN_X_NOT_REGISTERED, channel.c_str());
				return;
			}

			if (!source.AccessFor(ci).HasPriv("SAY") && !source.HasPriv("botserv/administration"))
			{
				source.Reply(ACCESS_DENIED);
				return;
			}

			if (!ci->bi)
			{
				source.Reply(BOT_NOT_ASSIGNED);
				return;
			}

			if (subcommand.equals_ci("ADD"))
			{
				if (url.empty())
				{
					this->OnSyntaxError(source, "");
					return;
				}
				this->fm.AddFeed(source, ci, url);
				return;
			}

			if (subcommand.equals_ci("DEL") || subcommand.equals_ci("DELETE") || subcommand.equals_ci("REMOVE"))
			{
				if (url.empty())
				{
					this->OnSyntaxError(source, "");
					return;
				}
				this->fm.RemoveFeed(source, ci, url);
				return;
			}

			if (subcommand.equals_ci("LIST"))
			{
				this->fm.ListFeeds(source, ci);
				return;
			}

			this->OnSyntaxError(source, "");
		}

		bool OnHelp(CommandSource& source, const Anope::string&) override
		{
			this->SendSyntax(source);
			source.Reply(" ");
			source.Reply(_("Adds, removes, or lists feed URLs for a channel."));
			return true;
		}
	};

	CommandBSFeeds commandbsfeeds;

	struct CurlBuffer final
	{
		std::string data;
		size_t max_bytes = kMaxFeedBytes;
	};

	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
	{
		auto* buffer = static_cast<CurlBuffer*>(userp);
		const size_t total = size * nmemb;
		if (!buffer || total == 0)
			return 0;

		const size_t space = (buffer->data.size() < buffer->max_bytes) ? (buffer->max_bytes - buffer->data.size()) : 0;
		const size_t to_copy = (total < space) ? total : space;
		if (to_copy > 0)
			buffer->data.append(static_cast<char*>(contents), to_copy);

		return total;
	}

	static Anope::string Trim(const Anope::string& in)
	{
		Anope::string out = in;
		out.trim();
		return out;
	}

	static bool LooksLikeUrl(const Anope::string& url)
	{
		return url.length() > 8 && (url.substr(0, 7).equals_ci("http://") || url.substr(0, 8).equals_ci("https://"));
	}

	static Anope::string HtmlEntityDecode(const Anope::string& in)
	{
		Anope::string out = in;
		out = out.replace_all_cs("&amp;", "&");
		out = out.replace_all_cs("&lt;", "<");
		out = out.replace_all_cs("&gt;", ">");
		out = out.replace_all_cs("&quot;", "\"");
		out = out.replace_all_cs("&apos;", "'");
		return out;
	}

	static std::string ToLowerCopy(const std::string& in)
	{
		std::string out;
		out.reserve(in.size());
		for (unsigned char ch : in)
			out.push_back(static_cast<char>(std::tolower(ch)));
		return out;
	}

	static bool FindTagBlockCI(const std::string& text, const std::string& tag, std::string& out, size_t* open_pos = nullptr)
	{
		const std::string lower = ToLowerCopy(text);
		const std::string lower_tag = ToLowerCopy(tag);
		const std::string open = "<" + lower_tag;
		const std::string close = "</" + lower_tag + ">";

		const size_t start = lower.find(open);
		if (start == std::string::npos)
			return false;
		const size_t open_end = lower.find('>', start);
		if (open_end == std::string::npos)
			return false;
		const size_t end = lower.find(close, open_end + 1);
		if (end == std::string::npos)
			return false;

		out = text.substr(open_end + 1, end - (open_end + 1));
		if (open_pos)
			*open_pos = start;
		return true;
	}

	static bool ExtractTagValueCI(const std::string& text, const std::string& tag, Anope::string& out)
	{
		std::string block;
		if (!FindTagBlockCI(text, tag, block))
			return false;
		out = block.c_str();
		return true;
	}

	static Anope::string StripCdata(const Anope::string& in)
	{
		static const Anope::string prefix = "<![CDATA[";
		static const Anope::string suffix = "]]>";
		if (in.length() >= prefix.length() + suffix.length() && in.substr(0, prefix.length()) == prefix)
		{
			const size_t start = prefix.length();
			const size_t end = in.rfind(suffix);
			if (end != Anope::string::npos && end >= start)
				return in.substr(start, end - start);
		}
		return in;
	}

	bool FetchUrl(const Anope::string& url, std::string& response_string)
	{
		CURL* curl = curl_easy_init();
		if (!curl)
		{
			Log() << "Feeds: CURL Initialization Error";
			return false;
		}

		CurlBuffer buffer;
		buffer.max_bytes = kMaxFeedBytes;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Anope-m_feeds/1.0");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			Log() << "Feeds: CURL Error: " << curl_easy_strerror(res);
			curl_easy_cleanup(curl);
			return false;
		}

		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		curl_easy_cleanup(curl);
		if (http_code != 200)
		{
			Log(LOG_DEBUG) << "Feeds: HTTP status " << http_code << " for URL: " << url;
			return false;
		}

		response_string = buffer.data;
		return true;
	}

	Anope::string FetchFeedTitle(const Anope::string& url)
	{
		std::string response_string;
		if (!FetchUrl(url, response_string))
			return "";

		Anope::string title;
		const Anope::string trimmed = Trim(response_string.c_str());
		if (!trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '['))
		{
			rapidjson::Document jsonData;
			if (jsonData.Parse(response_string.c_str()).HasParseError())
			{
				Log() << "Feeds: JSON Parse Error: " << rapidjson::GetParseError_En(jsonData.GetParseError());
				return "";
			}

			if (jsonData.IsObject() && jsonData.HasMember("title") && jsonData["title"].IsString())
				title = jsonData["title"].GetString();
			return title;
		}

		Anope::string raw_title;
		if (ExtractTagValueCI(response_string, "title", raw_title))
			title = raw_title;

		if (!title.empty())
			title = HtmlEntityDecode(Trim(title));

		return title;
	}

	struct FeedItem final
	{
		Anope::string feed_title;
		Anope::string item_title;
		Anope::string item_url;
		Anope::string item_id;
	};

	static Anope::string NormalizeXmlText(const Anope::string& in)
	{
		Anope::string out = HtmlEntityDecode(StripCdata(Trim(in)));
		return out;
	}

	static bool ExtractLinkHref(const std::string& text, Anope::string& out)
	{
		const std::string lower = ToLowerCopy(text);
		size_t pos = lower.find("<link");
		while (pos != std::string::npos)
		{
			const size_t tag_end = lower.find('>', pos);
			if (tag_end == std::string::npos)
				break;

			const size_t href_pos = lower.find("href", pos);
			if (href_pos != std::string::npos && href_pos < tag_end)
			{
				const size_t eq = lower.find('=', href_pos);
				if (eq != std::string::npos && eq < tag_end)
				{
					const size_t quote = text.find_first_of("\"'", eq + 1);
					if (quote != std::string::npos && quote < tag_end)
					{
						const char q = text[quote];
						const size_t end = text.find(q, quote + 1);
						if (end != std::string::npos && end > quote)
						{
							out = text.substr(quote + 1, end - quote - 1).c_str();
							return true;
						}
					}
				}
			}

		pos = lower.find("<link", tag_end);
		}

		return false;
	}

	bool ParseLatestItemFromJson(const std::string& response, FeedItem& item)
	{
		rapidjson::Document jsonData;
		if (jsonData.Parse(response.c_str()).HasParseError())
		{
			Log() << "Feeds: JSON Parse Error: " << rapidjson::GetParseError_En(jsonData.GetParseError());
			return false;
		}

		if (!jsonData.IsObject())
			return false;

		if (jsonData.HasMember("title") && jsonData["title"].IsString())
			item.feed_title = jsonData["title"].GetString();

		if (!jsonData.HasMember("items") || !jsonData["items"].IsArray() || jsonData["items"].Empty())
			return false;

		const auto& it = jsonData["items"][0];
		if (!it.IsObject())
			return false;

		if (it.HasMember("title") && it["title"].IsString())
			item.item_title = it["title"].GetString();
		if (it.HasMember("url") && it["url"].IsString())
			item.item_url = it["url"].GetString();
		else if (it.HasMember("external_url") && it["external_url"].IsString())
			item.item_url = it["external_url"].GetString();
		if (it.HasMember("id") && it["id"].IsString())
			item.item_id = it["id"].GetString();

		if (item.item_id.empty())
			item.item_id = !item.item_url.empty() ? item.item_url : item.item_title;

		return !item.item_id.empty();
	}

	bool ParseLatestItemFromXml(const std::string& response, FeedItem& item)
	{
		Anope::string feed_block;
		std::string channel_block;
		std::string feed_xml_block;
		if (FindTagBlockCI(response, "channel", channel_block))
			feed_xml_block = channel_block;
		else
			FindTagBlockCI(response, "feed", feed_xml_block);
		feed_block = feed_xml_block.c_str();

		Anope::string feed_title;
		if (!feed_xml_block.empty())
			ExtractTagValueCI(feed_xml_block, "title", feed_title);
		if (!feed_title.empty())
			item.feed_title = NormalizeXmlText(feed_title);

		std::string item_block;
		std::string entry_block;
		size_t item_pos = std::string::npos;
		size_t entry_pos = std::string::npos;
		const bool has_item = FindTagBlockCI(response, "item", item_block, &item_pos);
		const bool has_entry = FindTagBlockCI(response, "entry", entry_block, &entry_pos);
		if (!has_item && !has_entry)
			return false;

		const std::string& block = (has_item && (!has_entry || item_pos <= entry_pos)) ? item_block : entry_block;

		Anope::string title;
		if (ExtractTagValueCI(block, "title", title))
			item.item_title = NormalizeXmlText(title);

		Anope::string guid;
		if (ExtractTagValueCI(block, "guid", guid) || ExtractTagValueCI(block, "id", guid))
			item.item_id = NormalizeXmlText(guid);

		ExtractLinkHref(block, item.item_url);

		if (item.item_url.empty())
		{
			Anope::string link;
			if (ExtractTagValueCI(block, "link", link))
				item.item_url = NormalizeXmlText(link);
		}

		if (item.item_id.empty())
			item.item_id = !item.item_url.empty() ? item.item_url : item.item_title;

		return !item.item_id.empty();
	}

	bool FetchLatestItem(const Anope::string& url, FeedItem& item)
	{
		std::string response_string;
		if (!FetchUrl(url, response_string))
			return false;

		const Anope::string trimmed = Trim(response_string.c_str());
		if (!trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '['))
			return ParseLatestItemFromJson(response_string, item);

		return ParseLatestItemFromXml(response_string, item);
	}

	Anope::string BuildAnnounce(const Anope::string& feed, const Anope::string& title, const Anope::string& url) const
	{
		Anope::string out = this->announce_format;
		out = out.replace_all_cs("{feed}", feed.empty() ? "(feed)" : feed);
		out = out.replace_all_cs("{title}", title.empty() ? "(unknown)" : title);
		out = out.replace_all_cs("{url}", url);
		return out;
	}

	FeedChannelData* GetRecord(const Anope::string& name)
	{
		auto it = FeedChannelList->find(name);
		if (it == FeedChannelList->end())
			return nullptr;
		return it->second;
	}

	FeedChannelData& GetOrCreateRecord(const Anope::string& name)
	{
		if (auto* rec = GetRecord(name))
			return *rec;
		auto* rec = new FeedChannelData(name);
		return *rec;
	}

	void ScheduleDBSave()
	{
		if (Anope::ReadOnly)
			return;
		this->db_save_pending = true;
		if (this->db_save_timer)
			return;
		this->db_save_timer = new DeferredSaveTimer(*this, 5);
	}

	void MarkStateChanged()
	{
		if (!this->state)
			return;
		this->state->updated = Anope::CurTime;
		this->state->QueueUpdate();
		this->ScheduleDBSave();
	}

	void MarkRecordChanged(FeedChannelData& rec)
	{
		rec.updated = Anope::CurTime;
		rec.QueueUpdate();
		this->ScheduleDBSave();
	}

	void UpdateTotalFeeds()
	{
		if (!this->state)
			return;
		uint64_t total = 0;
		for (const auto& [_, rec] : *FeedChannelList)
		{
			if (!rec)
				continue;
			total += static_cast<uint64_t>(rec->feeds.size());
		}
		this->state->total_feeds = total;
		this->MarkStateChanged();
	}

	void AddFeed(CommandSource& source, ChannelInfo* ci, const Anope::string& raw_url)
	{
		Anope::string url = Trim(raw_url);
		if (!LooksLikeUrl(url))
		{
			source.Reply("Invalid URL. Only http(s) URLs are supported.");
			return;
		}

		FeedChannelData& rec = GetOrCreateRecord(ci->name);
		for (const auto& f : rec.feeds)
		{
			if (f.url.equals_ci(url))
			{
				source.Reply("Feed already exists for %s.", ci->name.c_str());
				return;
			}
		}

		FeedEntry entry;
		entry.url = url;
		entry.title = FetchFeedTitle(url);
		entry.added_by = source.GetNick();
		entry.added_at = Anope::CurTime;

		FeedItem latest;
		if (FetchLatestItem(url, latest))
		{
			if (!latest.feed_title.empty())
				entry.title = latest.feed_title;
			entry.last_item_id = latest.item_id;
			entry.last_item_url = latest.item_url;
			entry.last_item_title = latest.item_title;
			entry.last_seen = Anope::CurTime;
		}
		rec.feeds.push_back(entry);
		this->MarkRecordChanged(rec);
		this->UpdateTotalFeeds();

		const Anope::string announce = this->BuildAnnounce(entry.title, "Feed added", entry.url);
		if (ci->c && ci->c->FindUser(ci->bi))
		{
			IRCD->SendPrivmsg(*ci->bi, ci->name, announce);
			ci->bi->lastmsg = Anope::CurTime;
		}
		else
		{
			source.Reply("Feed added for %s: %s", ci->name.c_str(), entry.url.c_str());
		}

		bool override = !source.AccessFor(ci).HasPriv("SAY");
		Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, &this->commandbsfeeds, ci) << "to add feed " << entry.url;
	}

	void PollTick()
	{
		if (!Me || !Me->IsSynced())
			return;

		for (auto& [name, recp] : *FeedChannelList)
		{
			if (!recp)
				continue;
			FeedChannelData& rec = *recp;

			ChannelInfo* ci = ChannelInfo::Find(name);
			if (!ci || !ci->bi || !ci->c || !ci->c->FindUser(ci->bi))
				continue;

			unsigned int announced = 0;
			for (auto& f : rec.feeds)
			{
				if (announced >= this->max_per_poll)
					break;

				FeedItem latest;
				if (!FetchLatestItem(f.url, latest))
					continue;

				if (latest.item_id.empty())
					continue;

				if (!latest.feed_title.empty())
					f.title = latest.feed_title;

				if (f.last_item_id == latest.item_id)
				{
					f.last_seen = Anope::CurTime;
					continue;
				}

				f.last_item_id = latest.item_id;
				f.last_item_url = latest.item_url;
				f.last_item_title = latest.item_title;
				f.last_seen = Anope::CurTime;
				this->MarkRecordChanged(rec);

				const Anope::string msg = this->BuildAnnounce(f.title, latest.item_title, latest.item_url);
				IRCD->SendPrivmsg(*ci->bi, ci->name, msg);
				ci->bi->lastmsg = Anope::CurTime;
				++announced;
			}
		}
	}

	void RemoveFeed(CommandSource& source, ChannelInfo* ci, const Anope::string& raw_url)
	{
		Anope::string url = Trim(raw_url);
		FeedChannelData* recp = GetRecord(ci->name);
		if (!recp)
		{
			source.Reply("No feeds exist for %s.", ci->name.c_str());
			return;
		}

		auto& feeds = recp->feeds;
		auto it = std::find_if(feeds.begin(), feeds.end(), [&](const FeedEntry& f)
		{
			return f.url.equals_ci(url);
		});
		if (it == feeds.end())
		{
			source.Reply("Feed not found for %s.", ci->name.c_str());
			return;
		}

		Anope::string removed = it->url;
		feeds.erase(it);
		if (feeds.empty())
		{
			delete recp;
		}
		else
		{
			this->MarkRecordChanged(*recp);
		}
		this->UpdateTotalFeeds();

		source.Reply("Feed removed for %s: %s", ci->name.c_str(), removed.c_str());
		bool override = !source.AccessFor(ci).HasPriv("SAY");
		Log(override ? LOG_OVERRIDE : LOG_COMMAND, source, &this->commandbsfeeds, ci) << "to remove feed " << removed;
	}

	void ListFeeds(CommandSource& source, ChannelInfo* ci)
	{
		FeedChannelData* recp = GetRecord(ci->name);
		if (!recp || recp->feeds.empty())
		{
			source.Reply("No feeds exist for %s.", ci->name.c_str());
			return;
		}

		source.Reply("Feeds for %s:", ci->name.c_str());
		for (const auto& f : recp->feeds)
		{
			if (!f.title.empty())
				source.Reply("- %s (%s)", f.url.c_str(), f.title.c_str());
			else
				source.Reply("- %s", f.url.c_str());
		}
		source.Reply("End of FEEDS for %s.", ci->name.c_str());
	}

public:
	FeedsModule(const Anope::string& modname, const Anope::string& creator)
		: Module(modname, creator, VENDOR)
		, channel_type(this)
		, state_type(this)
		, commandbsfeeds(this, *this)
	{
		// Ensure a state record exists so db_json writes feeds.module.json.
		auto it = FeedsStateList->find("state");
		if (it != FeedsStateList->end())
			this->state = it->second;
		if (!this->state)
			this->state = new FeedsState();
		FeedsStateList->insert_or_assign(this->state->name, this->state);
	}

	~FeedsModule() override
	{
		this->db_save_pending = false;
		this->db_save_timer = nullptr;
	}

	void OnReload(Configuration::Conf& conf) override
	{
		const auto& modconf = conf.GetModule(this);
		this->announce_format = modconf.Get<Anope::string>("announce_format", "[{feed}] {title} - {url}");
		this->poll_interval = modconf.Get<time_t>("poll_interval", "300");
		this->max_per_poll = modconf.Get<unsigned int>("max_per_poll", "1");

		this->poll_timer.reset();
		if (this->poll_interval > 0)
			this->poll_timer = std::make_unique<FeedsPollTimer>(*this, this->poll_interval);

		this->MarkStateChanged();
	}
};

MODULE_INIT(FeedsModule)
