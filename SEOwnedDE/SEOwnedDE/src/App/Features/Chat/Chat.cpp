#include "../../../SDK/SDK.h"

#include "../CFG.h"
#include <string_view>

MAKE_SIGNATURE(CTFGameRules_ModifySentChat, "client.dll", "48 89 5C 24 ? 57 48 83 EC ? 80 B9 ? ? ? ? ? 41 8B F8", 0x0);

//pasted from mfed :muscle:
const std::vector<std::pair<std::string, std::string>> patterns
{
    { "r", "w" },
    { "l", "w" },
    { "R", "W" },
    { "L", "W" },
    { "n([aeiou])", "ny$1" },
    { "N([aeiou])", "Ny$1" },
    { "N([AEIOU])", "NY$1" }
};

const std::vector<std::string> suffixes
{
    " UwU", " OwO", " owo", " uwu", " nwn", " :3", " >w<", " ^w^", " <3"
};

std::string owoify(std::string text)
{
    for (auto &pattern : patterns)
    {
        std::regex reg(pattern.first);

        text = std::regex_replace(text, reg, pattern.second);
    }

    if (text.size() < 124)
    {
        int suffix{ I::UniformRandomStream ? I::UniformRandomStream->RandomInt(0, static_cast<int>(suffixes.size() - 1)) : 0 };

        text += suffixes[suffix];
    }

    return text;
}

std::string RewriteChatCommand(std::string_view command)
{
    const auto tokenStart = command.find_first_not_of(" \t");
    if (tokenStart == std::string_view::npos)
        return std::string(command);
    const auto tokenEnd = command.find_first_of(" \t", tokenStart);
    if (tokenEnd == std::string_view::npos)
        return std::string(command);
    const auto token = command.substr(tokenStart, tokenEnd - tokenStart);
    if (token != "say" && token != "say_team")
        return std::string(command);

    const auto textStart = command.find_first_not_of(" \t", tokenEnd);
    if (textStart == std::string_view::npos)
        return std::string(command);

    if (command[textStart] == '"')
    {
        // Only rewrite one complete chat command. A trailing command must
        // keep its original arguments and quoting.
        auto textEnd = textStart + 1;
        for (; textEnd < command.size(); ++textEnd)
        {
            if (command[textEnd] == '\\' && textEnd + 1 < command.size())
                ++textEnd;
            else if (command[textEnd] == '"')
                break;
        }
        if (textEnd == command.size() || textEnd == textStart + 1
            || command.find_first_not_of(" \t", textEnd + 1) != std::string_view::npos)
            return std::string(command);
        return std::string(command.substr(0, textStart + 1))
            + owoify(std::string(command.substr(textStart + 1, textEnd - textStart - 1)))
            + std::string(command.substr(textEnd));
    }

    if (command.find_first_of("\";\r\n", textStart) != std::string_view::npos)
        return std::string(command);
    return std::string(command.substr(0, textStart)) + "\""
        + owoify(std::string(command.substr(textStart))) + "\"";
}

MAKE_HOOK(IVEngineClient013_ClientCmd_Unrestricted, Memory::GetVFunc(I::EngineClient, 106), void, __fastcall,
    void* ecx, const char *szCmdString)
{
    if (CFG::Misc_Chat_Owoify && szCmdString)
    {
        const auto cmdString = RewriteChatCommand(szCmdString);
        return CALL_ORIGINAL(ecx, cmdString.c_str());
    }

    return CALL_ORIGINAL(ecx, szCmdString);
}

MAKE_HOOK(CTFGameRules_ModifySentChat, Signatures::CTFGameRules_ModifySentChat.Get(), void, __fastcall,
    void* ecx, char *pBuf, int iBufSize)
{
    static ConVar *tf_medieval_autorp{ I::CVar->FindVar("tf_medieval_autorp") };
    static ConVar *english{ I::CVar->FindVar("english") };

    if (CFG::Misc_Chat_Medieval && pBuf && iBufSize && tf_medieval_autorp && english)
    {
        struct s_CTFGameRules
        {
            char pad[1179]{};
            bool m_bPlayingMedieval{};
        };

        s_CTFGameRules *gamerules{ reinterpret_cast<s_CTFGameRules *>(ecx) };

        if (!tf_medieval_autorp || !english || !gamerules)
        {
            return CALL_ORIGINAL(ecx, pBuf, iBufSize);
        }

        bool originalEnglish{ english->GetBool() };
        bool originalAutoRP{ tf_medieval_autorp->GetBool() };
        bool originalPlaying{ gamerules->m_bPlayingMedieval };

        gamerules->m_bPlayingMedieval = true;
        tf_medieval_autorp->SetValue(true);
        english->SetValue(true);

        CALL_ORIGINAL(ecx, pBuf, iBufSize);

        gamerules->m_bPlayingMedieval = originalPlaying;
        tf_medieval_autorp->SetValue(originalAutoRP);
        english->SetValue(originalEnglish);

        return;
    }

    CALL_ORIGINAL(ecx, pBuf, iBufSize);
}
