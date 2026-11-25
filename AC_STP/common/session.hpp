#pragma once
#include "protocol.hpp"
#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <algorithm>

namespace stp {
using Send = std::function<bool(const Bytes&)>;
using Nonce = std::array<std::uint8_t, 16>;
class ClientSession {
public:
    Send send;
    std::atomic<bool> ready{false};
    std::atomic<std::uint32_t> progress{0};
    std::atomic<bool> integrity{true};
    std::atomic<std::uint32_t> generation{0};
    void SetIdentity(const Identity& i) { std::lock_guard<std::mutex> l(mutex_); identity_ = EncodeIdentity(i); }
    void Reset() {
        std::lock_guard<std::mutex> l(mutex_);
        ready = false; session_ = challenge_ = nextReport_ = 0; lastHello_ = lastResponse_ = lastSeen_ = 0;
        nonce_.clear(); queue_.clear(); helloSent_ = false; attempt_ = 0;
        ++generation;
    }
    bool Enqueue(const void* p, std::size_t n) {
        if (!p || n > MaxPayload) return false;
        Bytes b(static_cast<const std::uint8_t*>(p), static_cast<const std::uint8_t*>(p) + n);
        if (!ValidReport(b)) return false;
        std::lock_guard<std::mutex> l(mutex_);
        for (auto& r : queue_) if (r.payload == b) return true;
        if (queue_.size() >= 256) return false;
        queue_.push_back({std::move(b)}); return true;
    }
    void Tick(std::uint32_t now) {
        std::lock_guard<std::mutex> l(mutex_);
        if (session_ && Elapsed(now, lastSeen_) > TimeoutMs) {
            ready = false;
            // A temporary outage does not create a new server session. Preserve
            // report sequence numbers or the server would reject later reports.
            if (!helloSent_ || Elapsed(now, lastHello_) >= 3000) {
                send(Encode(Kind::Hello, 0, 0)); lastHello_ = now; helloSent_ = true;
            }
        }
        if (!session_) {
            if (!helloSent_ || Elapsed(now, lastHello_) >= 3000) {
                send(Encode(Kind::Hello, 0, 0)); lastHello_ = now; helloSent_ = true;
            }
            return;
        }
        if (!nonce_.empty() && !identity_.empty() && (!responseSent_ || Elapsed(now, lastResponse_) >= RetryMs(attempt_))) {
            Bytes payload = nonce_; payload.insert(payload.end(), identity_.begin(), identity_.end());
            Put32(payload, progress.load()); payload.push_back(integrity.load() ? 1 : 0);
            if (send(Encode(Kind::Response, session_, challenge_, payload))) {
                lastResponse_ = now; responseSent_ = true; if (attempt_ < 3) ++attempt_;
            }
        }
        if (!ready || queue_.empty() || (reportSent_ && Elapsed(now, lastReport_) < 63)) return;
        auto& r = queue_.front();
        if (!r.sent || Elapsed(now, r.lastSent) >= RetryMs(r.attempt)) {
            if (!r.sequence) r.sequence = ++nextReport_;
            if (send(Encode(Kind::Report, session_, r.sequence, r.payload))) {
                r.sent = true; r.lastSent = now; lastReport_ = now; reportSent_ = true; if (r.attempt < 3) ++r.attempt;
            }
        }
    }
    void Receive(const Frame& f, std::uint32_t now) {
        std::lock_guard<std::mutex> l(mutex_);
        if (f.kind == Kind::Challenge && f.session && f.sequence && f.payload.size() == 16) {
            if (session_ != f.session) {
                session_ = f.session; ready = false; nextReport_ = 0;
                ++generation;
                for (auto& r : queue_) { r.sequence = 0; r.sent = false; r.attempt = 0; }
            }
            if (challenge_ && f.sequence < challenge_) return;
            if (f.sequence != challenge_ || nonce_ != f.payload) {
                challenge_ = f.sequence; nonce_ = f.payload; responseSent_ = false; attempt_ = 0;
            }
            lastSeen_ = now;
        } else if (f.kind == Kind::Accepted && f.session == session_ && session_ && f.sequence == challenge_ && f.payload.empty()) {
            ready = true; nonce_.clear(); lastSeen_ = now;
        } else if (f.kind == Kind::ReportAck && f.session == session_ && session_ && f.payload.empty() && !queue_.empty() && f.sequence == queue_.front().sequence) {
            queue_.pop_front(); lastSeen_ = now;
        }
    }
private:
    struct Pending { Bytes payload; std::uint32_t sequence = 0, lastSent = 0; unsigned attempt = 0; bool sent = false; };
    std::mutex mutex_;
    Bytes identity_, nonce_;
    std::deque<Pending> queue_;
    std::uint32_t session_ = 0, challenge_ = 0, nextReport_ = 0, lastHello_ = 0, lastResponse_ = 0, lastSeen_ = 0;
    bool helloSent_ = false, responseSent_ = false;
    unsigned attempt_ = 0;
    std::uint32_t lastReport_ = 0;
    bool reportSent_ = false;
};

class ServerSession {
public:
    Send send;
    std::function<bool(std::uint32_t&, Nonce&)> random;
    std::function<void(const Identity&)> connected;
    std::function<void(const Bytes&)> report;
    std::function<void(bool)> health; // false = grace expired, true = recovered
    std::function<bool()> canAccept; // wait until the game server has created this player
    bool ready = false;
    void Reset() {
        ready = false; session_ = challenge_ = accepted_ = 0; pending_ = false; timedOut_ = false;
        lastValid_ = started_ = lastSend_ = lastChallenge_ = 0; attempts_ = 0; reports_ = {};
        rateStart_ = 0; rateCount_ = 0; haveProgress_ = false;
        peerStarted_ = false;
    }
    void Tick(std::uint32_t now) {
        if (!peerStarted_) { peerStarted_ = true; started_ = lastValid_ = now; }
        if (Elapsed(now, lastValid_) > TimeoutMs && !timedOut_) {
            timedOut_ = true; if (health) health(false);
        }
        if (!session_) {
            if (!random || !random(session_, nonce_) || !session_) { session_ = 0; return; }
            StartChallenge(now);
        }
        if (!pending_ && Elapsed(now, lastChallenge_) >= HeartbeatMs) StartChallenge(now);
        if (pending_ && (attempts_ == 0 || Elapsed(now, lastSend_) >= RetryMs(attempts_ - 1))) SendChallenge(now);
    }
    void Receive(const Frame& f, std::uint32_t now) {
        if (Elapsed(now, rateStart_) >= 1000) { rateStart_ = now; rateCount_ = 0; }
        if (++rateCount_ > 64) return;
        if (f.kind == Kind::Hello && !f.session && !f.sequence && f.payload.empty()) {
            if (!session_) {
                if (!random || !random(session_, nonce_) || !session_) { session_ = 0; return; }
                if (!peerStarted_) { peerStarted_ = true; started_ = lastValid_ = now; }
                StartChallenge(now);
            }
            if (pending_ && (!attempts_ || Elapsed(now, lastSend_) >= 1000)) SendChallenge(now);
            return;
        }
        if (!session_ || f.session != session_) return;
        if (f.kind == Kind::Response) {
            if (f.payload.size() != 99 || f.sequence != challenge_ || !std::equal(nonce_.begin(), nonce_.end(), f.payload.begin())) return;
            Identity identity;
            if (!DecodeIdentity(Bytes(f.payload.begin() + 16, f.payload.begin() + 94), identity) || identity.major != 9 || identity.minor < 11) return;
            if (f.payload[98] != 1) return; // code integrity failed; do not renew lease
            if (canAccept && !canAccept()) return;
            auto progress = Get32(f.payload.data() + 94);
            if (!haveProgress_ || progress != progress_) { progress_ = progress; progressAt_ = now; haveProgress_ = true; }
            if (Elapsed(now, progressAt_) > TimeoutMs) return;
            bool first = !ready;
            ready = true; pending_ = false; accepted_ = f.sequence; lastValid_ = now;
            send(Encode(Kind::Accepted, session_, accepted_));
            if (first && connected) connected(identity);
            if (timedOut_) { timedOut_ = false; if (health) health(true); }
            return;
        }
        if (f.kind == Kind::Report && ready && !timedOut_ && ValidReport(f.payload)) {
            if (reports_.Duplicate(f.sequence)) { send(Encode(Kind::ReportAck, session_, f.sequence)); return; }
            if (!reports_.Next(f.sequence)) return;
            reports_.last = f.sequence;
            if (report) report(f.payload);
            send(Encode(Kind::ReportAck, session_, f.sequence));
        }
    }
private:
    void StartChallenge(std::uint32_t now) {
        std::uint32_t unused = 0;
        if (!random || !random(unused, nonce_)) return;
        ++challenge_; if (!challenge_) ++challenge_;
        pending_ = true; attempts_ = 0; lastChallenge_ = now;
    }
    void SendChallenge(std::uint32_t now) {
        send(Encode(Kind::Challenge, session_, challenge_, Bytes(nonce_.begin(), nonce_.end())));
        lastSend_ = now; if (attempts_ < 4) ++attempts_;
    }
    Nonce nonce_{};
    ReportSequence reports_;
    std::uint32_t session_ = 0, challenge_ = 0, accepted_ = 0, lastValid_ = 0, started_ = 0, lastSend_ = 0, lastChallenge_ = 0;
    std::uint32_t rateStart_ = 0, progress_ = 0, progressAt_ = 0;
    unsigned attempts_ = 0, rateCount_ = 0;
    bool pending_ = false, timedOut_ = false, haveProgress_ = false;
    bool peerStarted_ = false;
};
}
