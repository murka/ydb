#include "grpc_session.h"

#include <ydb/library/yql/utils/log/log.h>

namespace NYql::NDqs {

TSession::~TSession() {
    TGuard<TMutex> lock(Mutex);
    for (auto actorId : Requests) {
        ActorSystem->Send(actorId, new NActors::TEvents::TEvPoison());
    }
}

void TSession::DeleteRequest(const NActors::TActorId& actorId)
{
    TGuard<TMutex> lock(Mutex);
    Requests.erase(actorId);
}

void TSession::AddRequest(const NActors::TActorId& actorId)
{
    TGuard<TMutex> lock(Mutex);
    Requests.insert(actorId);
}

TSessionStorage::TSessionStorage(
    NActors::TActorSystem* actorSystem,
    const NMonitoring::TDynamicCounters::TCounterPtr& sessionsCounter)
    : ActorSystem(actorSystem)
    , SessionsCounter(sessionsCounter)
{ }

void TSessionStorage::CloseSession(const TString& sessionId)
{
    TGuard<TMutex> lock(SessionMutex);
    auto it = Sessions.find(sessionId);
    if (it == Sessions.end()) {
        return;
    }
    SessionsByLastUpdate.erase(it->second.Iterator);
    Sessions.erase(it);
    *SessionsCounter = Sessions.size();
}

std::shared_ptr<TSession> TSessionStorage::GetSession(const TString& sessionId)
{
    Clean(TInstant::Now() - TDuration::Minutes(10));

    TGuard<TMutex> lock(SessionMutex);
    auto it = Sessions.find(sessionId);
    if (it == Sessions.end()) {
        return std::shared_ptr<TSession>();
    } else {
        SessionsByLastUpdate.erase(it->second.Iterator);
        SessionsByLastUpdate.push_back({TInstant::Now(), sessionId});
        it->second.Iterator = SessionsByLastUpdate.end();
        it->second.Iterator--;
        return it->second.Session;
    }
}

bool TSessionStorage::OpenSession(const TString& sessionId, const TString& username)
{
    TGuard<TMutex> lock(SessionMutex);
    if (Sessions.contains(sessionId)) {
        return false;
    }

    SessionsByLastUpdate.push_back({TInstant::Now(), sessionId});
    auto it = SessionsByLastUpdate.end(); --it;

    Sessions[sessionId] = TSessionAndIterator {
        std::make_shared<TSession>(username, ActorSystem),
        it
    };

    *SessionsCounter = Sessions.size();

    return true;
}

void TSessionStorage::Clean(TInstant before) {
    TGuard<TMutex> lock(SessionMutex);
    for (TSessionsByLastUpdate::iterator it = SessionsByLastUpdate.begin();
         it != SessionsByLastUpdate.end(); )
    {
        if (it->LastUpdate < before) {
            YQL_LOG(INFO) << "Drop session by timeout " << it->SessionId;
            Sessions.erase(it->SessionId);
            it = SessionsByLastUpdate.erase(it);
        } else {
            break;
        }
    }

    *SessionsCounter = Sessions.size();
}

void TSessionStorage::PrintInfo() const {
    YQL_LOG(INFO) << "SessionsByLastUpdate: " << SessionsByLastUpdate.size();
    YQL_LOG(DEBUG) << "Sessions: " << Sessions.size();
    ui64 currenSessionsCounter = *SessionsCounter;
    YQL_LOG(DEBUG) << "SessionsCounter: " << currenSessionsCounter;
}

} // namespace NYql::NDqs
