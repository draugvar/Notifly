//
// Tests for notifly::exchange.
//
// post_and_wait() covers the common shape: post one notification, wait for one
// reply, take the first that arrives. Every test here is a shape that shape
// cannot express -- the ones a real request/reply protocol keeps producing.
//
// Each test builds its own notifly so observer ids and per-notification type
// signatures can never collide with another test's.
//
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "notifly.h"

namespace
{
    using namespace std::chrono_literals;

    enum : int
    {
        kCommand = 2000,
        kStatus,
        kJobComplete,
        kJobCancelled,
        kTransferError,
        kTableEntry,
        kDocumentInserted
    };
}

// ---------------------------------------------------------------------------
// The plain case, the one post_and_wait() already covered.
// ---------------------------------------------------------------------------

TEST(exchange, the_first_delivery_completes_the_wait)
{
    notifly center;
    notifly::exchange ex(center);
    ex.on<int>(kStatus);

    std::thread device([&]
    {
        std::this_thread::sleep_for(20ms);
        center.post_notification(kStatus, 1);
    });

    EXPECT_EQ(ex.wait(2000ms), kStatus);
    device.join();
}

// ---------------------------------------------------------------------------
// A handler that judges each delivery instead of taking the first one.
// ---------------------------------------------------------------------------

TEST(exchange, a_predicate_ignores_the_state_the_sender_is_leaving)
{
    notifly center;
    int observed = -1;

    notifly::exchange ex(center);
    // Only a status reporting the state that was asked for ends the wait: a
    // device reports the state it is leaving first.
    ex.on<int>(kStatus, [&](const int a_state)
    {
        if(a_state != 1) return notifly_verdict::skip;
        observed = a_state;
        return notifly_verdict::done;
    });

    std::thread device([&]
    {
        std::this_thread::sleep_for(10ms);
        center.post_notification(kStatus, 0);   // leaving
        std::this_thread::sleep_for(10ms);
        center.post_notification(kStatus, 1);   // entering
    });

    EXPECT_EQ(ex.wait(2000ms), kStatus);
    EXPECT_EQ(observed, 1);
    EXPECT_EQ(ex.accepted(), 1u);   // the skipped delivery was not recorded
    device.join();
}

// ---------------------------------------------------------------------------
// A sender that repeats itself cannot disturb what the winning handler stored.
// ---------------------------------------------------------------------------

TEST(exchange, a_repeated_reply_cannot_disturb_the_winning_delivery)
{
    notifly center;
    int captured = 0;

    notifly::exchange ex(center);
    ex.on<int>(kStatus, [&](const int a_value)
    {
        captured = a_value;
        return notifly_verdict::done;
    });

    center.post_notification(kStatus, 7);

    // The sender answers again before the exchange is torn down.
    EXPECT_NO_THROW(center.post_notification(kStatus, 99));
    EXPECT_NO_THROW(center.post_notification(kStatus, 123));

    EXPECT_EQ(ex.wait(0ms), kStatus);
    EXPECT_EQ(captured, 7);
    EXPECT_EQ(ex.accepted(), 1u);
}

// ---------------------------------------------------------------------------
// Several notifications at once, and the caller learns which one answered.
// ---------------------------------------------------------------------------

TEST(exchange, the_caller_learns_which_notification_answered)
{
    notifly center;
    std::string outcome;

    notifly::exchange ex(center);
    ex.on<int>(kJobComplete, [&](int)
      {
          outcome = "complete";
          return notifly_verdict::done;
      })
      .on<std::string>(kJobCancelled, [&](const std::string& a_message)
      {
          outcome = a_message;
          return notifly_verdict::done;
      })
      .on<int>(kTransferError, [&](int)
      {
          outcome = "transfer error";
          return notifly_verdict::done;
      });

    center.post_notification(kJobCancelled, std::string("out of paper"));

    EXPECT_EQ(ex.wait(2000ms), kJobCancelled);
    EXPECT_EQ(outcome, "out of paper");
}

TEST(exchange, the_alternatives_that_did_not_answer_stay_out_of_the_way)
{
    notifly center;

    notifly::exchange ex(center);
    ex.on<int>(kJobComplete)
      .on<int>(kTransferError);

    center.post_notification(kJobComplete, 1);
    // The other alternative answers too, late. The first one still won.
    center.post_notification(kTransferError, 1);

    EXPECT_EQ(ex.wait(0ms), kJobComplete);
    EXPECT_EQ(ex.accepted(), 1u);
}

// ---------------------------------------------------------------------------
// Protocols where the sender only speaks up to refuse: silence is success.
// ---------------------------------------------------------------------------

TEST(exchange, silence_is_the_successful_outcome_when_only_failures_are_reported)
{
    notifly center;
    notifly::exchange ex(center);
    ex.on<int>(kTransferError);

    EXPECT_TRUE(ex.silent_for(80ms));
    EXPECT_EQ(ex.fired(), -1);
}

TEST(exchange, a_failure_arriving_within_the_window_breaks_the_silence)
{
    notifly center;
    notifly::exchange ex(center);
    ex.on<int>(kTransferError);

    std::thread device([&]
    {
        std::this_thread::sleep_for(10ms);
        center.post_notification(kTransferError, 42);
    });

    EXPECT_FALSE(ex.silent_for(2000ms));
    device.join();
}

// ---------------------------------------------------------------------------
// A reply the sender streams in pieces, whose length the protocol never states.
// ---------------------------------------------------------------------------

TEST(exchange, drain_collects_a_streamed_reply_until_the_sender_goes_quiet)
{
    notifly center;
    std::mutex entries_mutex;
    std::vector<int> entries;

    notifly::exchange ex(center);
    ex.on<int>(kTableEntry, [&](const int a_entry)
    {
        std::lock_guard lock(entries_mutex);
        entries.push_back(a_entry);
        // Never completes: only the sender going quiet ends this.
        return notifly_verdict::keep;
    });

    std::thread device([&]
    {
        for(int i = 1; i <= 5; ++i)
        {
            std::this_thread::sleep_for(10ms);
            center.post_notification(kTableEntry, i);
        }
    });

    const auto count = ex.drain(150ms, 5000ms);
    device.join();

    EXPECT_EQ(count, 5u);

    std::lock_guard lock(entries_mutex);
    EXPECT_EQ(entries, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(exchange, drain_gives_up_at_the_deadline_when_the_sender_never_goes_quiet)
{
    notifly center;
    std::atomic_bool stop{false};

    notifly::exchange ex(center);
    ex.on<int>(kTableEntry, [](int) { return notifly_verdict::keep; });

    std::thread device([&]
    {
        while(!stop.load())
        {
            center.post_notification(kTableEntry, 1);
            std::this_thread::sleep_for(5ms);
        }
    });

    const auto started = std::chrono::steady_clock::now();
    // The quiet window is wide enough that it never closes; the deadline is
    // what has to stop this.
    const auto count = ex.drain(5000ms, 150ms);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    stop.store(true);
    device.join();

    EXPECT_GT(count, 0u);
    EXPECT_LT(elapsed, 3000ms);
}

TEST(exchange, drain_ends_early_when_a_handler_says_done)
{
    notifly center;

    notifly::exchange ex(center);
    ex.on<int>(kTableEntry, [](const int a_entry)
    {
        // A sender that does mark its last entry.
        return a_entry < 0 ? notifly_verdict::done : notifly_verdict::keep;
    });

    std::thread device([&]
    {
        center.post_notification(kTableEntry, 1);
        center.post_notification(kTableEntry, 2);
        center.post_notification(kTableEntry, -1);   // end of table
    });

    const auto started = std::chrono::steady_clock::now();
    const auto count = ex.drain(5000ms, 10000ms);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    device.join();

    EXPECT_EQ(count, 3u);
    EXPECT_EQ(ex.fired(), kTableEntry);
    EXPECT_LT(elapsed, 3000ms);   // neither the quiet window nor the deadline was needed
}

// ---------------------------------------------------------------------------
// Waiting for something no command asked for.
// ---------------------------------------------------------------------------

TEST(exchange, an_exchange_can_wait_for_something_no_command_asked_for)
{
    notifly center;
    std::string document;

    notifly::exchange ex(center);
    ex.on<std::string>(kDocumentInserted, [&](const std::string& a_document)
    {
        document = a_document;
        return notifly_verdict::done;
    });

    // Nothing is posted by the waiter: the document turns up when whoever is at
    // the machine decides to insert one.
    std::thread player([&]
    {
        std::this_thread::sleep_for(30ms);
        center.post_notification(kDocumentInserted, std::string("ticket-42"));
    });

    EXPECT_EQ(ex.wait(5000ms), kDocumentInserted);
    EXPECT_EQ(document, "ticket-42");
    player.join();
}

// ---------------------------------------------------------------------------
// Lifetime and error reporting.
// ---------------------------------------------------------------------------

TEST(exchange, destruction_unsubscribes_every_handler)
{
    notifly center;
    int deliveries = 0;

    {
        notifly::exchange ex(center);
        ex.on<int>(kStatus, [&](int)
        {
            ++deliveries;
            return notifly_verdict::keep;
        });

        center.post_notification(kStatus, 1);
    }

    // With the exchange gone the notification has no observers left at all.
    EXPECT_EQ(center.post_notification(kStatus, 2),
              static_cast<int>(notifly_result::notification_not_found));
    EXPECT_EQ(deliveries, 1);
}

TEST(exchange, a_type_mismatch_is_reported_instead_of_waiting_for_a_reply_that_cannot_arrive)
{
    notifly center;

    const auto existing = center.add_observer(kStatus, [](const std::string&) {});
    ASSERT_GT(existing, 0);

    notifly::exchange ex(center);
    ex.on<int>(kStatus);   // wrong payload shape for this notification

    EXPECT_EQ(ex.status(), notifly_result::payload_type_not_match);

    center.remove_observer(existing);
}

TEST(exchange, a_healthy_set_of_subscriptions_reports_success)
{
    notifly center;

    notifly::exchange ex(center);
    ex.on<int>(kJobComplete)
      .on<std::string>(kJobCancelled)
      .on<int>(kTransferError);

    EXPECT_EQ(ex.status(), notifly_result::success);
}

// ---------------------------------------------------------------------------
// capture() is what post_and_wait() is built on: on() plus writing the first
// delivery straight into a variable instead of a handler. It is public on its
// own, for a caller building a multi-alternative exchange who wants that
// shorthand for one of the branches without writing out the lambda.
// ---------------------------------------------------------------------------

TEST(exchange, capture_writes_the_first_delivery_into_a_single_value)
{
    notifly center;
    int status = -1;

    notifly::exchange ex(center);
    ex.capture(kStatus, status);

    center.post_notification(kStatus, 7);

    EXPECT_EQ(ex.wait(0ms), kStatus);
    EXPECT_EQ(status, 7);
}

TEST(exchange, capture_writes_a_multi_argument_delivery_into_a_tuple)
{
    notifly center;
    std::tuple<int, std::string> job;

    notifly::exchange ex(center);
    ex.capture(kJobComplete, job);

    center.post_notification(kJobComplete, 42, std::string("ticket-42"));

    EXPECT_EQ(ex.wait(0ms), kJobComplete);
    EXPECT_EQ(std::get<0>(job), 42);
    EXPECT_EQ(std::get<1>(job), "ticket-42");
}

// ---------------------------------------------------------------------------
// post_and_wait() rides on the same machinery, so it inherits the guard.
// ---------------------------------------------------------------------------

TEST(exchange, post_and_wait_keeps_the_first_answer_when_the_sender_answers_twice)
{
    notifly center;

    const auto responder = center.add_observer(kCommand, [&](const int a_value)
    {
        // Answers twice, the way a device reporting the state it leaves and
        // then the state it enters does.
        center.post_notification(kStatus, a_value * 2);
        center.post_notification(kStatus, a_value * 3);
    });
    ASSERT_GT(responder, 0);

    int result = 0;
    notifly_result ret{};
    EXPECT_NO_THROW(ret = center.post_and_wait(kCommand, kStatus, 2000, result, 21));

    EXPECT_EQ(ret, notifly_result::success);
    EXPECT_EQ(result, 42);   // the first answer, not the second

    center.remove_observer(responder);
}

TEST(exchange, post_and_wait_still_reports_a_missing_responder)
{
    notifly center;

    int result = 0;
    const auto ret = center.post_and_wait(kCommand, kStatus, 100, result, 1);

    EXPECT_EQ(ret, notifly_result::notification_not_found);
}
