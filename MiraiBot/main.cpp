#include "MiraiBot.h"
#include <jsoncpp/json/json.h>
using namespace std;
using namespace Cyan;

struct MemberWarn {
	int64_t qId;
	unsigned warns;
};

struct GroupWarn {
	int64_t GroupId;

	unsigned int WarnLimit;
	short WarnOperation;
	int WarnMuteTime;
	vector<MemberWarn> MemberWarns;

	int GetMemberWarns(int64_t qId) {
		for (vector<MemberWarn>::iterator iter = MemberWarns.begin(); iter != MemberWarns.end(); iter++) {
			if (qId == (*iter).qId)
				return (*iter).warns;
		}
		return 0;
	}
	bool SetMemberWarns(int64_t qId, unsigned int val) {
		for (vector<MemberWarn>::iterator iter = MemberWarns.begin(); iter != MemberWarns.end(); iter++) {
			if (val == 0)(*iter).warns = 0;
			else (*iter).warns += val;
			return true;
		}
		MemberWarns.push_back({ qId,(val == -1 ? 1 : val) });
		return true;
	}

	void Default() {
		WarnMuteTime = 600;
		WarnOperation = 0;
		WarnLimit = 10;
		MemberWarns.clear();
	}
};

struct GroupPerm {
	int64_t GroupId;
	vector<int64_t>	PermGroups;
	bool CheckPerm(int64_t qId) {
		for (vector<int64_t>::iterator iter = PermGroups.begin(); iter != PermGroups.end(); iter++) {
			if (qId == *iter)
				return true;
		}
		return false;
	}
	void RevokePerm(int64_t qId) {
		for (vector<int64_t>::iterator iter = PermGroups.begin(); iter != PermGroups.end(); iter++) {
			if (qId == *iter) {
				PermGroups.erase(iter);
				return;
			}
		}
	}
	void Default() {
		PermGroups.clear();
	}
};

vector<int64_t>					StoppedGroups;
vector<GroupWarn>				GroupWarns;
vector<GroupPerm>				GroupPerms;
mutex							GroupLock;

MiraiBot						bot;
SessionOptions					opts;

string							command;
string							WarnOperText[] = { "None","Notice","Mute","Kick" };

string getTime()
{
	time_t timep;
	time(&timep);
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
	return tmp;
}

bool CheckGroup(int64_t groupId) {
	for (vector<int64_t>::iterator iter = StoppedGroups.begin(); iter != StoppedGroups.end(); iter++) {
		if (groupId == *iter)
			return true;
	}
	return false;
}

bool EnableGroup(int64_t groupId) {
	for (vector<int64_t>::iterator iter = StoppedGroups.begin(); iter != StoppedGroups.end(); iter++) {
		if (groupId == *iter) {
			StoppedGroups.erase(iter);
			return true;
		}
	}
	return false;
}

int GetGroupWarnIter(int64_t groupId) {
	for (int i = 0; i < GroupWarns.size(); i++) {
		if (groupId == GroupWarns[i].GroupId) {
			return i;
		}
	}
	return -1;
}

int GetGroupPermIter(int64_t groupId) {
	for (int i = 0; i < GroupPerms.size(); i++) {
		if (groupId == GroupPerms[i].GroupId) {
			return i;
		}
	}
	return -1;
}

void signalCallback(int sig) {
	switch (sig) {
	case 2: {
		bot.Disconnect();
		printf("Exit.\n");
		exit(0);
	}
	}
}

void SaveData() {
	GroupLock.lock();
	Json::Value root;
	Json::StyledStreamWriter writer;
	ofstream ofs("data.json");
	for (vector<int64_t>::iterator iter = StoppedGroups.begin(); iter != StoppedGroups.end(); iter++) {
		root["DisabledGroups"].append((long long)(*iter));
	}
	for (int i = 0; i < GroupWarns.size(); i++) {
		root["WarnGroups"][i]["GroupId"] = (long long)GroupWarns[i].GroupId;
		root["WarnGroups"][i]["WarnLimit"] = GroupWarns[i].WarnLimit;
		root["WarnGroups"][i]["WarnOperation"] = GroupWarns[i].WarnOperation;
		root["WarnGroups"][i]["WarnMuteTime"] = GroupWarns[i].WarnMuteTime;
		for (int j = 0; j < GroupWarns[i].MemberWarns.size(); j++) {
			root["WarnGroups"][i]["MemberWarns"][j]["qId"] = (long long)GroupWarns[i].MemberWarns[j].qId;
			root["WawrnGroups"][i]["MemberWarns"][j]["warns"] = (long long)GroupWarns[i].MemberWarns[j].warns;
		}
	}
	for (int i = 0; i < GroupPerms.size(); i++) {
		root["PermissionGroups"][i]["GroupId"] = (long long)GroupPerms[i].GroupId;
		for (int j = 0; j < GroupPerms[i].PermGroups.size(); j++) 
			root["PermissionGroups"][i]["Members"].append((long long)(GroupPerms[i].PermGroups[j]));
	}
	writer.write(ofs, root);
	ofs.close();
	GroupLock.unlock();
}

void ReadData() {
	GroupLock.lock();
	Json::Value root;
	Json::Reader reader;
	ifstream ifs("data.json");
	reader.parse(ifs, root);

	for (int i = 0; i < root["DisabledGroups"].size(); i++)
		StoppedGroups.push_back(root["DisabledGroups"][i]	.asInt64());
	for (int i = 0; i < root["WarnGroups"].size(); i++) {
		GroupWarn mG;
		mG.Default();
		mG.GroupId = root["WarnGroups"][i]["GroupId"].asInt64();
		mG.WarnLimit = root["WarnGroups"][i]["WarnLimit"].asInt();
		mG.WarnOperation = root["WarnGroups"][i]["WarnOperation"].asInt();
		mG.WarnMuteTime = root["WarnGroups"][i]["WarnMuteTime"].asInt();
		for (int j = 0; j < root["WawrnGroups"][i]["MemberWarns"].size(); j++) {
			MemberWarn mM;
			mM.qId = root["WarnGroups"][i]["MemberWarns"][j]["qId"].asInt64();
			mM.warns = root["WarnGroups"][i]["MemberWarns"][j]["warns"].asInt();
			mG.MemberWarns.push_back(mM);
		}
		GroupWarns.push_back(mG);
	}
	for (int i = 0; i < root["PermissionGroups"].size(); i++) {
		GroupPerm mG;
		mG.Default();
		mG.GroupId = root["PermissionGroups"][i]["GroupId"].asInt64();
		for (int j = 0; j < root["PermissionGroups"][i]["Members"].size(); j++) 
			mG.PermGroups.push_back(root["PermissionGroups"][i]["Members"][j].asInt64());
		GroupPerms.push_back(mG);
	}
	ifs.close();
	GroupLock.unlock();
}

int main(int argc, char* argv[])
{

	signal(SIGINT, signalCallback);

	bool onreload = false;

	ReadData();

	opts = SessionOptions::FromCommandLine(argc, argv);
	bot.Connect(opts);
	cout << "Bot working..." << endl;
	bot.On<FriendMessage>(
		[&](FriendMessage m)
		{
			m.Reply(m.MessageChain);
		});
	bot.OnEventReceived<NewFriendRequestEvent>(
		[&](NewFriendRequestEvent newFriend)
		{
			newFriend.Accept();
		});
	bot.On<NudgeEvent>(
		[&](NudgeEvent e)
		{
			if (e.FromId == bot.GetBotQQ()) return;
			if (e.Target != bot.GetBotQQ()) return;
			bot.SendNudge(e.FromId, *e.GetSubjectId());
		});
	bot.On<GroupMessage>(//For Administrators;
		[&](GroupMessage m) {
			istringstream strMessage(m.MessageChain.GetPlainText());
			vector<AtMessage> atMessages = m.MessageChain.GetAll<AtMessage>();
			string out[16];

			int n = 0;
			while (strMessage >> out[n]) {
				n++;
			}

			int64_t gid = m.Sender.Group.GID.ToInt64();
			int64_t qid = m.Sender.QQ.ToInt64();
			int WarnIter = GetGroupWarnIter(gid);
			int PermIter = GetGroupPermIter(gid);

			bool isAdmin = !(m.Sender.Permission == GroupPermission::Member);
			bool isDisabled = CheckGroup(gid);
			bool isAllowed = (PermIter == -1 ? true : GroupPerms[PermIter].CheckPerm(qid));

			if (PermIter == -1) {
				vector<int64_t> t;
				GroupPerms.push_back({ gid,t });
				PermIter = GroupPerms.size() - 1;
			}
			GroupLock.lock();
			if (out[0] == "!!status")
				m.QuoteReply(MessageChain().Plain(CheckGroup(gid) ? "Bot is disabled." : "Bot is working..."));
			if (out[0] == "!!mute" && (isAdmin || isAllowed) && !(isDisabled)) {
				if (out[1] == "help") {
					m.QuoteReply(MessageChain().Plain("权限：>Member\n作用：禁言成员\n语法：\n - !!mute help  //输出帮助信息\n - !!mute @成员1 @成员2 ... 禁言时间（s）  //设置禁言\n - !!mute all  //设置全员禁言"));
				}
				else if (out[1] == "all") {
					bot.MuteAll(m.Sender.Group.GID);
					printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)开启全体禁言.\n",
						getTime().c_str(),
						m.Sender.Group.Name.c_str(), gid,
						m.Sender.MemberName.c_str(), qid);
				}
				for (unsigned int i = 0; i < atMessages.size(); i++) {
					bot.Mute(m.Sender.Group.GID, atMessages[i].Target(), (unsigned)atoi(out[n - 1].c_str()));
					printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)禁言成员%lld %d秒.\n",
						getTime().c_str(),
						m.Sender.Group.Name.c_str(), gid,
						m.Sender.MemberName.c_str(), qid,
						atMessages[i].Target().ToInt64(),
						atoi(out[n - 1].c_str()));
				}
			}
			if (out[0] == "!!unmute" && (isAdmin || isAllowed) && !(isDisabled)) {
				if (out[1] == "help") {
					m.QuoteReply(MessageChain().Plain("权限：>Member\n作用：解禁成员\n语法：\n - !!unmute help  //输出帮助信息\n - !!unmute @成员1 @成员2 ...   //解禁成员\n - !!unmute all 解除全员禁言"));
				}
				if (out[1] == "all") {
					bot.UnMuteAll(m.Sender.Group.GID);
					printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)解除全体禁言.\n",
						getTime().c_str(),
						m.Sender.Group.Name.c_str(), gid,
						m.Sender.MemberName.c_str(), qid);
				}
				for (unsigned int i = 0; i < atMessages.size(); i++) {
					bot.UnMute(m.Sender.Group.GID, atMessages[i].Target());
					printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)解除禁言%lld.\n",
						getTime().c_str(),
						m.Sender.Group.Name.c_str(), gid,
						m.Sender.MemberName.c_str(), qid,
						atMessages[i].Target().ToInt64());
				}
			}
			if (out[0] == "!!kick" && (isAdmin || isAllowed) && !(isDisabled)) {
				if (out[1] == "help") {
					m.QuoteReply(MessageChain().Plain("权限：>Member\n作用：踢出成员\n语法：\n - !!kick help  //输出帮助信息\n - !!kick @成员  //踢出成员"));
				}
				for (unsigned int i = 0; i < atMessages.size(); i++) {
					bot.Kick(m.Sender.Group.GID, atMessages[i].Target());
					printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)踢出成员%lld.\n",
						getTime().c_str(),
						m.Sender.Group.Name.c_str(), gid,
						m.Sender.MemberName.c_str(), qid,
						atMessages[i].Target().ToInt64());
				}
			}
			if (out[0] == "!!warn" && !(isDisabled)) {
				if (WarnIter == -1) {
					GroupWarn mG;
					mG.Default();
					mG.GroupId = gid;
					GroupWarns.push_back(mG);
					WarnIter = GroupWarns.size() - 1;
				}
				if (out[1] == "help") {
					m.QuoteReply(MessageChain().Plain("权限：\n - help info get:All Members\n - set clear add:>Member\n\
作用：警告\n\
语法：\n - !!warn help  //输出帮助信息\n - !!warn info  //获取该功能相应设置\n - !!warn get  //获取成员警告信息\n - !!warn set limit 上限数值   //设置警告上限\n - !!warn set operation 操作类型(none notice mute kick)   //设置达到上限操作（Mute操作需设置时长）\n - !!warn add  @成员1 @成员2 ... 增加数   //增加成员warns\n - !!warn clear @成员1 @成员2 ...    //清零成员warns"));
				}
				if (out[1] == "info") {
					char infostr[1024];
					sprintf(infostr, "当前设置：\n警告上限：%d\n到达上限的操作：%s", GroupWarns[WarnIter].WarnLimit, WarnOperText[GroupWarns[WarnIter].WarnOperation].c_str());
					if (GroupWarns[WarnIter].WarnOperation == 2) {
						sprintf(infostr, "%s\n时长：%d秒", infostr, GroupWarns[WarnIter].WarnMuteTime);
					}
					m.QuoteReply(MessageChain().Plain(infostr));
				}
				if (out[1] == "get") {
					for (unsigned int i = 0; i < atMessages.size(); i++) {
						char infostr[1024];
						sprintf(infostr, "成员：%lld\n警告次数：%d", atMessages[i].Target().ToInt64(), GroupWarns[WarnIter].GetMemberWarns(atMessages[i].Target().ToInt64()));
						m.QuoteReply(MessageChain().Plain(infostr));
					}
				}
				if (isAdmin || isAllowed) {
					if (out[1] == "set") {
						if (out[2] == "limit") {
							int limit = atoi(out[3].c_str());
							if (limit > 0) {
								GroupWarns[WarnIter].WarnLimit = limit;
								m.QuoteReply(MessageChain().Plain("设置成功"));
								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)设置警告上限为%d次. \n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid,
									limit);
							}
							else {
								m.QuoteReply(MessageChain().Plain("设置失败：上限必须为大于0的正整数"));
							}
						}
						if (out[2] == "operation") {
							if (out[3] == "none") {
								GroupWarns[WarnIter].WarnOperation = 0;
								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)设置警告操作为None. \n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid);
							}
							if (out[3] == "notice") {
								GroupWarns[WarnIter].WarnOperation = 1;
								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)设置警告操作为Notice. \n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid);
							}
							if (out[3] == "mute") {
								int SetTime = atoi(out[4].c_str());
								if (SetTime == 0) {
									m.QuoteReply(MessageChain().Plain("设置失败：无效禁言时长"));
									GroupLock.unlock();
									return;
								}
								GroupWarns[WarnIter].WarnMuteTime = SetTime;
								GroupWarns[WarnIter].WarnOperation = 2;
								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)设置警告操作为Mute. \n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid);
							}
							if (out[3] == "kick") {
								GroupWarns[WarnIter].WarnOperation = 3;

								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)设置警告操作为Kick. \n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid);
							}
							{
								m.QuoteReply(MessageChain().Plain("设置失败：未知操作"));
								GroupLock.unlock();
								return;
							}
							m.QuoteReply(MessageChain().Plain("设置成功."));
						}
					}
					if (out[1] == "clear") {
						for (unsigned int i = 0; i < atMessages.size(); i++) {
							GroupWarns[WarnIter].SetMemberWarns(atMessages[i].Target().ToInt64(), 0);
							printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)清空成员%lld的警告数. \n",
								getTime().c_str(),
								m.Sender.Group.Name.c_str(), gid,
								m.Sender.MemberName.c_str(), qid,
								atMessages[i].Target().ToInt64());
						}
						char infostr[1024];
						sprintf(infostr, "已清零%d个群成员的警告次数.", atMessages.size());
						m.QuoteReply(MessageChain().Plain(infostr));
					}
					if (out[1] == "add") {
						int success = 0;
						for (unsigned int i = 0; i < atMessages.size(); i++) {
							if (atMessages[i].Target().ToInt64() == bot.GetBotQQ().ToInt64()) {
								char infostr[1024];
								sprintf(infostr, "错误：无法对机器人执行操作！");
								m.QuoteReply(MessageChain().Plain(infostr));
								continue;
							}
							int val = atoi(out[n - 1].c_str());
							if (val == 0)val = 1;
							GroupWarns[WarnIter].SetMemberWarns(atMessages[i].Target().ToInt64(), val);
							printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)增加成员%lld的警告数:%d次 \n",
								getTime().c_str(),
								m.Sender.Group.Name.c_str(), gid,
								m.Sender.MemberName.c_str(), qid,
								atMessages[i].Target().ToInt64(),
								val);
							int targetWarns = GroupWarns[WarnIter].GetMemberWarns(atMessages[i].Target().ToInt64());
							if (targetWarns >= GroupWarns[WarnIter].WarnLimit) {
								GroupWarns[WarnIter].SetMemberWarns(atMessages[i].Target().ToInt64(), targetWarns % GroupWarns[WarnIter].WarnLimit);
								switch (GroupWarns[WarnIter].WarnOperation) {
								case 1: {
									char infostr[1024];
									sprintf(infostr, "成员%lld已达到警告上限！", atMessages[i].Target().ToInt64());
									bot.SendMessage(m.Sender.Group.GID, MessageChain().Plain(infostr));
									printf("[I][%s][GroupMessage][%s(%lld)]成员%lld的警告数已达到上限. \n",
										getTime().c_str(),
										m.Sender.Group.Name.c_str(), gid,
										atMessages[i].Target().ToInt64());
									break;
								}
								case 2: {
									char infostr[1024];
									sprintf(infostr, "成员%lld已达到警告上限！\n执行操作：Mute %ds.", atMessages[i].Target().ToInt64(), GroupWarns[WarnIter].WarnMuteTime);
									bot.SendMessage(m.Sender.Group.GID, MessageChain().Plain(infostr));
									bot.Mute(m.Sender.Group.GID, atMessages[i].Target(), GroupWarns[WarnIter].WarnMuteTime);
									printf("[I][%s][GroupMessage][%s(%lld)]成员%lld的警告数已达到上限.执行禁言. \n",
										getTime().c_str(),
										m.Sender.Group.Name.c_str(), gid,
										atMessages[i].Target().ToInt64());
									break;
								}
								case 3: {
									char infostr[1024];
									sprintf(infostr, "成员%lld已达到警告上限！\n执行操作：Kick.", atMessages[i].Target().ToInt64(), GroupWarns[WarnIter].WarnMuteTime);
									bot.SendMessage(m.Sender.Group.GID, MessageChain().Plain(infostr));
									bot.Kick(m.Sender.Group.GID, atMessages[i].Target());
									printf("[I][%s][GroupMessage][%s(%lld)]成员%lld的警告数已达到上限.执行踢出. \n",
										getTime().c_str(),
										m.Sender.Group.Name.c_str(), gid,
										atMessages[i].Target().ToInt64());
									break;
								}
								}
							}
							success++;
						}
						char infostr[1024];
						sprintf(infostr, "已增加%d个群成员的警告次数.", success);
						m.QuoteReply(MessageChain().Plain(infostr));
					}
				}
			}
			if (out[0] == "!!bot" && (isAdmin || isAllowed)) {
				if (out[1] == "help") {
					m.QuoteReply(MessageChain().Plain("权限：>Member\n作用：Bot基本操作\n语法：\n - !!bot help  //输出帮助信息\n - !!bot reload  //重加载\n - !!bot disable  //禁止Bot在本群活动\r\n - !!bot enable  //允许Bot在本群活动"));
					return;
				}
				if (out[1] == "reload") {
					bot.Disconnect();
					printf("[I][%s]管理员%s(%lld)重载机器人.\n",
						getTime().c_str(),
						m.Sender.MemberName.c_str(), qid);
					GroupLock.unlock();
					onreload = true;
					return;
				}
				if (out[1] == "disable") {
					int64_t groupId = gid;
					if (!CheckGroup(groupId)) {
						StoppedGroups.push_back(groupId);
						m.QuoteReply(MessageChain().Plain("Bot在本群停止活动."));
						printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)禁止机器人在群组%lld活动.\n",
							getTime().c_str(),
							m.Sender.Group.Name.c_str(), gid,
							m.Sender.MemberName.c_str(), qid,
							gid);
					}
				}
				if (out[1] == "enable") {
					int64_t groupId = gid;
					if (CheckGroup(groupId)) {
						EnableGroup(groupId);
						m.QuoteReply(MessageChain().Plain("Bot已被允许在本群活动."));
						printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)允许机器人在群组%lld活动.\n",
							getTime().c_str(),
							m.Sender.Group.Name.c_str(), gid,
							m.Sender.MemberName.c_str(), qid),
							gid;
					}
				}
			}
			if (out[0] == "!!perm" && (isAdmin || isAllowed) && !(isDisabled)) {
				if (out[1] == "help") {
					m.QuoteReply(MessageChain().Plain("权限：\n - help list:All Members\n - add revoke:>Member\n\
作用：操作Bot权限组设置\n语法：\n - !!perm help  //输出帮助信息\n - !!perm list   //获取当前权限组成员列表\n - !!perm add @成员1 @成员2 ...    //授权成员\n - !!perm revoke @成员1 @成员2 ...    //撤销成员权限"));
				}
				if (out[1] == "list") {
					char infostr[1024];
					sprintf(infostr, "已授权成员:");
					for (unsigned int i = 0; i < GroupPerms[PermIter].PermGroups.size(); i++)
						sprintf(infostr, "%s\n%lld", infostr, GroupPerms[PermIter].PermGroups[i]);
					m.QuoteReply(MessageChain().Plain(infostr));
				}
				if (isAdmin || isAllowed) {
					if (out[1] == "add") {
						int success = 0;
						for (unsigned int i = 0; i < atMessages.size(); i++) {
							int64_t TargetQid = atMessages[i].Target().ToInt64();
							if (!GroupPerms[PermIter].CheckPerm(TargetQid)) {
								success++;
								GroupPerms[PermIter].PermGroups.push_back(TargetQid);
								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)授权成员%lld操作机器人.\n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid,
									TargetQid);
							}
						}
						char infostr[1024];
						sprintf(infostr, "已授权%d个成员.", success);
						m.QuoteReply(MessageChain().Plain(infostr));
					}
					if (out[1] == "revoke") {
						int success = 0;
						for (unsigned int i = 0; i < atMessages.size(); i++) {
							int64_t TargetQid = atMessages[i].Target().ToInt64();
							if (GroupPerms[PermIter].CheckPerm(TargetQid)) {
								success++;
								GroupPerms[PermIter].RevokePerm(TargetQid);
								printf("[I][%s][GroupMessage][%s(%lld)]管理员%s(%lld)撤销成员%lld操作机器人权限.\n",
									getTime().c_str(),
									m.Sender.Group.Name.c_str(), gid,
									m.Sender.MemberName.c_str(), qid,
									TargetQid);
							}
						}
						char infostr[1024];
						sprintf(infostr, "已撤销%d个成员的权限.", success);
						m.QuoteReply(MessageChain().Plain(infostr));
					}
				}
			}
			GroupLock.unlock();
			return;
		});

	while (!onreload) {
		MiraiBot::SleepSeconds(5);
		SaveData();
	}
	SaveData();
	printf("Exit.");
	return 0;
}

