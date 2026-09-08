#include "conditions/ClauseReorder.h"
#include "workbench/ActionDrop.h"
#include "workbench/ConditionDrop.h"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using namespace sfs::workbench::condition_drop;

int g_failures = 0;

void Expect(const bool a_condition, const std::string_view a_message) {
  if (a_condition) {
    return;
  }
  std::cerr << "FAIL: " << a_message << '\n';
  ++g_failures;
}

Target Row(const std::uint64_t a_identity) {
  return {TargetKind::ConditionalRow, a_identity};
}

Target Rule(const std::uint64_t a_identity) {
  return {TargetKind::ConditionalVisibilityRule, a_identity};
}

void TestCatalogAssignment() {
  const std::vector<Entry> entries{{Row(1), ""}};
  const auto plan = BuildPlan(entries, {.target = Row(1),
                                        .conditionId = "condition-a"});
  Expect(plan.Changed() && plan.changes.size() == 1,
         "catalog drop must plan one target mutation");
  Expect(plan.changes[0].target == Row(1) &&
             plan.changes[0].conditionId == "condition-a",
         "catalog drop must preserve the stable target identity");
}

void TestMoveAndSwap() {
  {
    const std::vector<Entry> entries{{Row(1), "condition-a"}, {Row(2), ""}};
    const auto plan = BuildPlan(
        entries, {.target = Row(2),
                  .conditionId = "condition-a",
                  .source = Row(1)});
    Expect(plan.Changed() && plan.changes.size() == 2,
           "moving to an empty target must be one two-sided transaction");
    Expect(plan.changes[0].target == Row(1) &&
               plan.changes[0].conditionId.empty() &&
               plan.changes[1].target == Row(2) &&
               plan.changes[1].conditionId == "condition-a",
           "moving to an empty target must clear only the source");
  }
  {
    const std::vector<Entry> entries{{Row(1), "condition-a"},
                                     {Rule(2), "condition-b"}};
    const auto plan = BuildPlan(
        entries, {.target = Rule(2),
                  .conditionId = "condition-a",
                  .source = Row(1)});
    Expect(plan.Changed() && plan.changes.size() == 2,
           "occupied cross-kind drop must produce an atomic swap");
    Expect(plan.changes[0].target == Row(1) &&
               plan.changes[0].conditionId == "condition-b" &&
               plan.changes[1].target == Rule(2) &&
               plan.changes[1].conditionId == "condition-a",
           "occupied cross-kind drop must retain both conditions");
  }
}

void TestRejectedDropsDoNotPlanPartialChanges() {
  const std::vector<Entry> entries{{Row(1), "condition-a"}, {Rule(2), ""}};

  auto plan = BuildPlan(entries, {.target = Rule(2),
                                  .conditionId = "condition-stale",
                                  .source = Row(1)});
  Expect(plan.status == Status::StaleSource && plan.changes.empty(),
         "a stale source payload must fail without partial mutations");

  plan = BuildPlan(entries, {.target = Rule(99),
                             .conditionId = "condition-a",
                             .source = Row(1)});
  Expect(plan.status == Status::TargetNotFound && plan.changes.empty(),
         "a missing target must fail without clearing the source");

  plan = BuildPlan(entries, {.target = Rule(2),
                             .conditionId = "condition-a",
                             .source = Row(99)});
  Expect(plan.status == Status::SourceNotFound && plan.changes.empty(),
         "a missing source must fail without changing the target");

  plan = BuildPlan(entries, {.target = Row(1),
                             .conditionId = "condition-a",
                             .source = Row(1)});
  Expect(plan.status == Status::NoChange && plan.changes.empty(),
         "dropping onto the same stable identity must be a no-op");

  plan = BuildPlan(entries, {.target = {TargetKind::Catalog, 1},
                             .conditionId = "condition-a"});
  Expect(plan.status == Status::InvalidRequest && plan.changes.empty(),
         "the catalog cannot be used as a state destination");
}

void TestAmbiguousIdentityFailsClosed() {
  const std::vector<Entry> entries{{Row(1), "condition-a"},
                                   {Row(1), "condition-b"}};
  const auto plan = BuildPlan(entries, {.target = Row(1),
                                        .conditionId = "condition-c"});
  Expect(plan.status == Status::AmbiguousIdentity && plan.changes.empty(),
         "duplicate stable identities must fail closed");
}

void TestDiagnosticNamesAreStable() {
  Expect(StatusName(Status::Applied) == "applied" &&
             StatusName(Status::NoChange) == "no-change" &&
             StatusName(Status::InvalidRequest) == "invalid-request" &&
             StatusName(Status::SourceNotFound) == "source-not-found" &&
             StatusName(Status::TargetNotFound) == "target-not-found" &&
             StatusName(Status::StaleSource) == "stale-source" &&
             StatusName(Status::AmbiguousIdentity) == "ambiguous-identity" &&
             StatusName(Status::Conflict) == "conflict",
         "every rejected condition drop must have a stable diagnostic name");
}

void TestActionDropEndpointValidation() {
  using sfs::workbench::action_drop::Entry;
  using sfs::workbench::action_drop::Request;
  using sfs::workbench::action_drop::ValidateRequest;

  const std::vector<Entry> entries{
      {.target = Row(10), .formIDs = {0x100, 0x101}},
      {.target = Rule(20), .formIDs = {0x200}},
      {.target = Rule(30), .formIDs = {}}};

  auto status = ValidateRequest(
      entries, Request{.target = Rule(30), .formID = 0x100, .source = Row(10)});
  Expect(status == Status::Applied,
         "an action move must validate both stable endpoints before staging");

  status = ValidateRequest(
      entries, Request{.target = Rule(30), .formID = 0x999, .source = Row(10)});
  Expect(status == Status::StaleSource,
         "a stale action payload must be rejected before target mutation");

  status = ValidateRequest(
      entries, Request{.target = Rule(99), .formID = 0x100, .source = Row(10)});
  Expect(status == Status::TargetNotFound,
         "a missing action target must not allow source cleanup");

  status = ValidateRequest(
      entries, Request{.target = Row(10), .formID = 0x100, .source = Row(10)});
  Expect(status == Status::NoChange,
         "an action dropped on its own stable endpoint must be a no-op");

  status = ValidateRequest(
      entries, Request{.target = Rule(30), .formID = 0x100,
                       .source = Row(99)});
  Expect(status == Status::SourceNotFound,
         "a missing action source must be rejected before staging");

  const std::vector<Entry> ambiguousEntries{
      {.target = Row(10), .formIDs = {0x100}},
      {.target = Rule(30), .formIDs = {}},
      {.target = Rule(30), .formIDs = {0x200}}};
  status = ValidateRequest(
      ambiguousEntries,
      Request{.target = Rule(30), .formID = 0x100, .source = Row(10)});
  Expect(status == Status::AmbiguousIdentity,
         "duplicate action target identities must fail closed");

  status = ValidateRequest(entries,
                           Request{.target = Rule(30), .formID = 0});
  Expect(status == Status::InvalidRequest,
         "an action payload without a form must never stage a mutation");
}

void TestClauseReorderTransaction() {
  namespace clause_reorder = sfs::conditions::clause_reorder;
  std::vector<sfs::conditions::Clause> clauses(3);
  clauses[0].functionName = "first";
  clauses[1].functionName = "second";
  clauses[2].functionName = "third";

  const clause_reorder::Payload firstPayload{
      .sourceIndex = 0,
      .sourceFingerprint = clause_reorder::Fingerprint(clauses[0])};
  auto status =
      clause_reorder::ApplyMoveTransaction(clauses, firstPayload, 3);
  Expect(status == clause_reorder::Status::Applied &&
             clauses[0].functionName == "second" &&
             clauses[1].functionName == "third" &&
             clauses[2].functionName == "first",
         "a delivered clause drop must commit the complete reorder once");

  const auto beforeStale = clauses;
  status = clause_reorder::ApplyMoveTransaction(clauses, firstPayload, 1);
  Expect(status == clause_reorder::Status::StaleSource &&
             clauses[0].functionName == beforeStale[0].functionName &&
             clauses[1].functionName == beforeStale[1].functionName &&
             clauses[2].functionName == beforeStale[2].functionName,
         "a stale clause payload must leave the complete draft unchanged");

  const clause_reorder::Payload currentPayload{
      .sourceIndex = 2,
      .sourceFingerprint = clause_reorder::Fingerprint(clauses[2])};
  status = clause_reorder::ApplyMoveTransaction(clauses, currentPayload, 3);
  Expect(status == clause_reorder::Status::NoChange,
         "dropping a clause into its current slot boundary must be a no-op");

  const auto beforeInvalidTarget = clauses;
  status = clause_reorder::ApplyMoveTransaction(clauses, currentPayload, 4);
  Expect(status == clause_reorder::Status::InvalidTarget &&
             clauses.size() == beforeInvalidTarget.size() &&
             clauses[0].functionName == beforeInvalidTarget[0].functionName &&
             clauses[1].functionName == beforeInvalidTarget[1].functionName &&
             clauses[2].functionName == beforeInvalidTarget[2].functionName,
         "an out-of-range clause slot must leave the draft untouched");
}
} // namespace

int main() {
  TestCatalogAssignment();
  TestMoveAndSwap();
  TestRejectedDropsDoNotPlanPartialChanges();
  TestAmbiguousIdentityFailsClosed();
  TestDiagnosticNamesAreStable();
  TestActionDropEndpointValidation();
  TestClauseReorderTransaction();

  if (g_failures != 0) {
    std::cerr << g_failures << " condition drop logic test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Condition drop transaction tests passed\n";
  return EXIT_SUCCESS;
}
