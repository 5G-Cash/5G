// Copyright (c) 2018 Tadhg Riordan Zcoin Developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fivegnodeman.h"
#include "main.h"
#include "init.h"
#include "util.h"
#include "client-api/server.h"
#include "client-api/protocol.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "fivegnode-sync.h"
#include "wallet/wallet.h"
#include "fivegnode.h"
#include "fivegnodeconfig.h"
#include "fivegnodeman.h"
#include "activefivegnode.h"
#include <zmqserver/zmqabstract.h>
#include "univalue.h"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/range/algorithm/remove_if.hpp>

namespace fs = boost::filesystem;
using namespace boost::chrono;
using namespace std;

/* Parse help string as JSON object.
*/
void parseHelpString(UniValue& result, std::string helpString)
{
    std::vector<std::string> categoriesVec;
    iter_split(categoriesVec, helpString, boost::algorithm::first_finder("\n\n"));

    BOOST_FOREACH(std::string category, categoriesVec){
        UniValue categoryArr(UniValue::VARR);
        std::vector<std::string> categoryVec;
        boost::split(categoryVec, category, boost::is_any_of("\n"), boost::token_compress_on);

        std::string categoryKey = categoryVec[0];
        categoryKey.erase(boost::remove_if(categoryKey, boost::is_any_of("= ")), categoryKey.end());

        for(unsigned fiveg=1; index<categoryVec.size(); index++){
           categoryArr.push_back(categoryVec[index]);
        }
        result.push_back(Pair(categoryKey,categoryArr));
    }
}

/**
 * (Taken from Qt. Could expose the function there but it's likely we will deprecate Qt in the future)
 * Split shell command line into a list of arguments. Aims to emulate \c bash and friends.
 *
 * - Arguments are delimited with whitespace
 * - Extra whitespace at the beginning and end and between arguments will be ignored
 * - Text can be "double" or 'single' quoted
 * - The backslash \c \ is used as escape character
 *   - Outside quotes, any character can be escaped
 *   - Within double quotes, only escape \c " and backslashes before a \c " or another backslash
 *   - Within single quotes, no escaping is possible and no special interpretation takes place
 *
 * @param[out]   args        Parsed arguments will be appended to this list
 * @param[in]    strCommand  Command line to split
 */
bool parseFromCommandLine(std::vector<std::string> &args, const std::string &strCommand)
{
    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED
    } state = STATE_EATING_SPACES;
    std::string curarg;
    BOOST_FOREACH(char ch, strCommand)
    {
        switch(state)
        {
        case STATE_ARGUMENT: // In or after argument
        case STATE_EATING_SPACES: // Handle runs of whitespace
            switch(ch)
            {
            case '"': state = STATE_DOUBLEQUOTED; break;
            case '\'': state = STATE_SINGLEQUOTED; break;
            case '\\': state = STATE_ESCAPE_OUTER; break;
            case ' ': case '\n': case '\t':
                if(state == STATE_ARGUMENT) // Space ends argument
                {
                    args.push_back(curarg);
                    curarg.clear();
                }
                state = STATE_EATING_SPACES;
                break;
            default: curarg += ch; state = STATE_ARGUMENT;
            }
            break;
        case STATE_SINGLEQUOTED: // Single-quoted string
            switch(ch)
            {
            case '\'': state = STATE_ARGUMENT; break;
            default: curarg += ch;
            }
            break;
        case STATE_DOUBLEQUOTED: // Double-quoted string
            switch(ch)
            {
            case '"': state = STATE_ARGUMENT; break;
            case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
            default: curarg += ch;
            }
            break;
        case STATE_ESCAPE_OUTER: // '\' outside quotes
            curarg += ch; state = STATE_ARGUMENT;
            break;
        case STATE_ESCAPE_DOUBLEQUOTED: // '\' in double-quoted text
            if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
            curarg += ch; state = STATE_DOUBLEQUOTED;
            break;
        }
    }
    switch(state) // final state
    {
    case STATE_EATING_SPACES:
        return true;
    case STATE_ARGUMENT:
        args.push_back(curarg);
        return true;
    default: // ERROR to end in one of the other states
        return false;
    }
}

UniValue apistatus(Type type, const UniValue& data, const UniValue& auth, bool fHelp)
{
#ifdef ENABLE_WALLET
    LOCK2(cs_main, pwalletMain ? &pwalletMain->cs_wallet : NULL);
#else
    LOCK(cs_main);
#endif

    UniValue obj(UniValue::VOBJ);
    UniValue modules(UniValue::VOBJ);
    
    modules.push_back(Pair("API", !APIIsInWarmup()));
    modules.push_back(Pair("Fivegnode", fivegnodeSync.IsSynced()));

    obj.push_back(Pair("version", CLIENT_VERSION));
    obj.push_back(Pair("protocolVersion", PROTOCOL_VERSION));
    if (pwalletMain) {
        obj.push_back(Pair("walletVersion", pwalletMain->GetVersion()));
        obj.push_back(Pair("walletLock",    pwalletMain->IsCrypted()));
        if(nWalletUnlockTime>0){
            obj.push_back(Pair("unlockedUntil", nWalletUnlockTime));
        }
    }

    UniValue fivegnode(UniValue::VOBJ);
    fivegnode.push_back(Pair("localCount", fivegnodeConfig.getCount()));
    fivegnode.push_back(Pair("totalCount", mnodeman.CountFivegnodes()));
    fivegnode.push_back(Pair("enabledCount", mnodeman.CountEnabled()));
    obj.push_back(Pair("Fivegnode", fivegnode));

    obj.push_back(Pair("dataDir",       GetDataDir(true).string()));
    obj.push_back(Pair("network",       ChainNameFromCommandLine()));
    obj.push_back(Pair("blocks",        (int)chainActive.Height()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("devAuth",       CZMQAbstract::DEV_AUTH));
    obj.push_back(Pair("synced",        fivegnodeSync.GetBlockchainSynced()));
    obj.push_back(Pair("reindexing",    fReindex || !fivegnodeSync.GetBlockchainSynced()));
    obj.push_back(Pair("safeMode",      GetWarnings("api") != ""));

#ifdef WIN32
    obj.push_back(Pair("pid",           (int)GetCurrentProcessId()));
#else
    obj.push_back(Pair("pid",           getpid()));
#endif
    obj.push_back(Pair("modules",       modules));

    return obj;
}

UniValue backup(Type type, const UniValue& data, const UniValue& auth, bool fHelp)
{
    string directory = find_value(data, "directory").get_str();

    milliseconds secs = duration_cast< milliseconds >(
        system_clock::now().time_since_epoch()
    );
    UniValue firstSeenAt = secs.count();
    string filename = "index_backup-" + to_string(firstSeenAt.get_int64()) + ".zip";

    fs::path backupPath (directory);
    backupPath /= filename;

    vector<string> filePaths;
    vector<string> folderPaths;

    filePaths.push_back(DEFAULT_WALLET_DAT);
    folderPaths.push_back(PERSISTENT_FILENAME);

    if(!CreateZipFile(GetDataDir().string() + "/", folderPaths, filePaths, backupPath.string())){
        throw JSONAPIError(API_MISC_ERROR, "Failed to create backup");
    }

    return true;
}

UniValue stop(Type type, const UniValue& data, const UniValue& auth, bool fHelp)
{
    // Accept the deprecated and ignored 'detach' boolean argument
    if (fHelp)
        throw runtime_error(
            "stop\n"
            "\nStop FivegXProject server.");
    // Event loop will exit after current HTTP requests have been handled, so
    // this reply will get back to the client.
    StartShutdown();
    return true;
}

UniValue rpc(Type type, const UniValue& data, const UniValue& auth, bool fHelp)
{
    switch(type){
        case Initial: {
            // call help command and parse. No data here.
            UniValue request(UniValue::VOBJ);
            UniValue reply(UniValue::VOBJ);
            UniValue result(UniValue::VOBJ);
            
            std::string method = "help";
            std::vector<std::string> args;
            UniValue params = RPCConvertValues(method, args);

            request.push_back(Pair("method", method));
            request.push_back(Pair("params", params));

            reply = JSONRPCExecOne(request);

            UniValue categories(UniValue::VOBJ);
            std::string replyStr = find_value(reply, "result").get_str();
            parseHelpString(categories, replyStr);

            result.push_back(Pair("categories", categories));
            return result;
        }
        case Create: {
            UniValue request(UniValue::VOBJ);
            UniValue reply(UniValue::VOBJ);

            std::string method = find_value(data, "method").get_str();
            std::string argsStr = find_value(data, "args").get_str();

            std::vector<std::string> args;
            parseFromCommandLine(args, argsStr);

            UniValue params = RPCConvertValues(method, args);

            request.push_back(Pair("method", method));
            request.push_back(Pair("params", params));

            reply = JSONRPCExecOne(request);

            return reply;
        }
        default: {
            throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
        }
    }
    return true;

}

static const CAPICommand commands[] =
{ //  category              collection         actor (function)          authPort   authPassphrase   warmupOk
  //  --------------------- ------------       ----------------          -------- --------------   --------
    { "misc",               "apiStatus",       &apistatus,               false,     false,           true   },
    { "misc",               "backup",          &backup,                  true,      false,           false  },
    { "misc",               "rpc",             &rpc,                     true,      false,           false  },
    { "misc",               "stop",            &stop,                    true,      false,           false  }
};

void RegisterMiscAPICommands(CAPITable &tableAPI)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableAPI.appendCommand(commands[vcidx].collection, &commands[vcidx]);
}
