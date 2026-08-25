/*
 *  exchange_example.cpp
 *  notifly
 *
 *  Driving a request/reply protocol with notifly::exchange.
 *
 *  post_notification() is one-way and add_observer() is open-ended, so talking
 *  to something that answers means hand-rolling the join: subscribe, send, wait,
 *  unsubscribe -- and get the ordering right, because a reply can come back
 *  before the send call has even returned.
 *
 *  post_and_wait() does that for the common shape: one request, one reply, take
 *  the first that arrives. This example covers the shapes it cannot express,
 *  each one modelled on something a real device does:
 *
 *    1. a reply worth ignoring   -- the device reports the state it is leaving
 *                                   before the state it is entering
 *    2. several possible answers -- a job ends in completion, refusal or a jam,
 *                                   and the caller needs to know which
 *    3. silence means success    -- the device only speaks up to refuse
 *    4. a reply sent in pieces   -- a table whose length the protocol never states
 *    5. nothing to send at all   -- waiting on a person, not on a command
 */

#include "notifly.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{
    enum : int
    {
        cmd_enable = 100,
        cmd_print,
        cmd_read_table,

        evt_status,
        evt_job_complete,
        evt_job_refused,
        evt_paper_jam,
        evt_table_entry,
        evt_note_inserted
    };

    /// Stands in for the device: it answers commands on a thread of its own,
    /// the way a real one answers over a wire.
    class fake_device
    {
    public:
        explicit fake_device(notifly& a_center) : m_center(a_center)
        {
            m_observers.push_back(m_center.add_observer(cmd_enable, [this](int)
            {
                // Real hardware announces the state it is leaving first, then
                // the one it settles into.
                m_center.post_notification(evt_status, 0);
                std::this_thread::sleep_for(30ms);
                m_center.post_notification(evt_status, 1);
            }));

            // Note the by-value parameter: post_notification() deduces its
            // arguments by value, and a notification's payload shape has to
            // match exactly, references included -- an observer taking
            // "const std::string&" registers a different shape and is never
            // called. See get_type_string() in notifly.h.
            m_observers.push_back(m_center.add_observer(cmd_print, [this](const std::string a_ticket)
            {
                std::this_thread::sleep_for(20ms);
                if(a_ticket.empty()) m_center.post_notification(evt_job_refused, std::string("empty ticket"));
                else m_center.post_notification(evt_job_complete, 7);
            }));

            m_observers.push_back(m_center.add_observer(cmd_read_table, [this](int)
            {
                // The protocol never says how many entries there are; the
                // device simply stops sending.
                for(int i = 1; i <= 4; ++i)
                {
                    std::this_thread::sleep_for(15ms);
                    m_center.post_notification(evt_table_entry, i * 5);
                }
            }));
        }

        ~fake_device()
        {
            for(const int observer: m_observers) m_center.remove_observer(observer);
        }

        fake_device(const fake_device&) = delete;
        fake_device& operator=(const fake_device&) = delete;

    private:
        notifly& m_center;
        std::vector<int> m_observers;
    };
}

// ---------------------------------------------------------------------------
// 1. A reply worth ignoring.
// ---------------------------------------------------------------------------

void enable_the_device(notifly& a_center)
{
    printf("\n1. enable -- ignoring the state the device is leaving\n");

    notifly::exchange ex(a_center);

    // post_and_wait() would take that first "0" as the answer and report the
    // device enabled while it is still on its way there. A verdict lets the
    // handler say "not this one" and stay subscribed.
    ex.on<int>(evt_status, [](const int a_state)
    {
        printf("   device reports state %d\n", a_state);
        return a_state == 1 ? notifly_verdict::done : notifly_verdict::skip;
    });

    a_center.post_notification(cmd_enable, 0);

    if(ex.wait(2000ms) < 0) printf("   -> timed out\n");
    else printf("   -> enabled (took %zu of the deliveries)\n", ex.accepted());
}

// ---------------------------------------------------------------------------
// 2. Several possible answers, and the caller needs to know which one came.
// ---------------------------------------------------------------------------

void print_a_ticket(notifly& a_center, const std::string& a_ticket)
{
    printf("\n2. print -- three ways for one job to end\n");

    notifly::exchange ex(a_center);

    int transaction = 0;
    std::string refusal;

    ex.on<int>(evt_job_complete, [&](const int a_transaction)
      {
          transaction = a_transaction;
          return notifly_verdict::done;
      })
      .on<std::string>(evt_job_refused, [&](const std::string& a_reason)
      {
          refusal = a_reason;
          return notifly_verdict::done;
      })
      .on<int>(evt_paper_jam, [](int) { return notifly_verdict::done; });

    a_center.post_notification(cmd_print, a_ticket);

    // A print job runs for seconds, so it is given far longer than a command.
    switch(const int fired = ex.wait(30000ms))
    {
        case evt_job_complete: printf("   -> printed, transaction %d\n", transaction); break;
        case evt_job_refused:  printf("   -> refused: %s\n", refusal.c_str()); break;
        case evt_paper_jam:    printf("   -> paper jam\n"); break;
        default:               printf("   -> timed out (fired=%d)\n", fired); break;
    }
}

// ---------------------------------------------------------------------------
// 3. Silence means success.
// ---------------------------------------------------------------------------

void transfer_a_template(notifly& a_center)
{
    printf("\n3. transfer -- the device only speaks up to refuse\n");

    notifly::exchange ex(a_center);
    ex.on<std::string>(evt_job_refused);

    // Nothing is sent here: this stands for a transfer the device accepts
    // silently. Waiting for a reply that only exists on failure would always
    // time out, so the question is inverted -- did anything object?
    if(ex.silent_for(200ms)) printf("   -> accepted (nothing objected)\n");
    else printf("   -> refused\n");
}

// ---------------------------------------------------------------------------
// 4. A reply sent in pieces.
// ---------------------------------------------------------------------------

void read_the_note_table(notifly& a_center)
{
    printf("\n4. read table -- a reply of unstated length\n");

    std::vector<int> entries;

    notifly::exchange ex(a_center);
    ex.on<int>(evt_table_entry, [&](const int a_value)
    {
        entries.push_back(a_value);
        // keep, not done: there is no way to know which entry is the last.
        return notifly_verdict::keep;
    });

    a_center.post_notification(cmd_read_table, 0);

    // Ends once the device has been quiet for 100ms, or at 5s regardless.
    const auto count = ex.drain(100ms, 5000ms);

    printf("   -> %zu entries:", count);
    for(const int entry: entries) printf(" %d", entry);
    printf("\n");
}

// ---------------------------------------------------------------------------
// 5. Nothing to send at all.
// ---------------------------------------------------------------------------

void wait_for_a_note(notifly& a_center)
{
    printf("\n5. wait for a note -- no command to post\n");

    std::string note;

    notifly::exchange ex(a_center);
    ex.on<std::string>(evt_note_inserted, [&](const std::string& a_note)
    {
        note = a_note;
        return notifly_verdict::done;
    });

    // Whoever is at the machine, not a command, decides when this happens.
    std::thread player([&a_center]
    {
        std::this_thread::sleep_for(50ms);
        a_center.post_notification(evt_note_inserted, std::string("EUR 20"));
    });

    if(ex.wait(30000ms) < 0) printf("   -> nobody inserted anything\n");
    else printf("   -> got %s\n", note.c_str());

    player.join();
}

// ---------------------------------------------------------------------------

int main()
{
    notifly center;
    const fake_device device(center);

    enable_the_device(center);
    print_a_ticket(center, "<ticket/>");
    print_a_ticket(center, "");
    transfer_a_template(center);
    read_the_note_table(center);
    wait_for_a_note(center);

    printf("\n");
    return 0;
}
