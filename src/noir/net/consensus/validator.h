#pragma once
#include <noir/net/types.h>

namespace noir::net::consensus { // todo - move consensus somewhere (maybe under libs?)

struct validator {
  bytes address;
  bytes pub_key;
  int64_t voting_power;
  int64_t proposer_priority;
};

struct validator_set {
  std::vector<validator> validators;
  validator proposer;
  int64_t total_voting_power = 0;

  void update_with_change_set() {
// UpdateWithChangeSet attempts to update the validator set with 'changes'.
// It performs the following steps:
// - validates the changes making sure there are no duplicates and splits them in updates and deletes
// - verifies that applying the changes will not result in errors
// - computes the total voting power BEFORE removals to ensure that in the next steps the priorities
//   across old and newly added validators are fair
// - computes the priorities of new validators against the final set
// - applies the updates against the validator set
// - applies the removals against the validator set
// - performs scaling and centering of priority values
// If an error is detected during verification steps, it is returned and the validator set
// is not changed.
#if 0
    if len(changes) == 0 {
        return nil
    }

    // Check for duplicates within changes, split in 'updates' and 'deletes' lists (sorted).
    updates, deletes, err := processChanges(changes)
    if err != nil {
        return err
    }

    if !allowDeletes && len(deletes) != 0 {
        return fmt.Errorf("cannot process validators with voting power 0: %v", deletes)
    }

    // Check that the resulting set will not be empty.
    if numNewValidators(updates, vals) == 0 && len(vals.Validators) == len(deletes) {
        return errors.New("applying the validator changes would result in empty set")
    }

    // Verify that applying the 'deletes' against 'vals' will not result in error.
    // Get the voting power that is going to be removed.
    removedVotingPower, err := verifyRemovals(deletes, vals)
    if err != nil {
        return err
    }

    // Verify that applying the 'updates' against 'vals' will not result in error.
    // Get the updated total voting power before removal. Note that this is < 2 * MaxTotalVotingPower
    tvpAfterUpdatesBeforeRemovals, err := verifyUpdates(updates, vals, removedVotingPower)
    if err != nil {
        return err
    }

    // Compute the priorities for updates.
    compute_new_priorities(updates, vals, tvpAfterUpdatesBeforeRemovals)

    // Apply updates and removals.
    vals.applyUpdates(updates)
    vals.applyRemovals(deletes)

    vals.updateTotalVotingPower() // will panic if total voting power > MaxTotalVotingPower

    // Scale and center.
    vals.RescalePriorities(PriorityWindowSizeFactor * vals.TotalVotingPower())
    vals.shiftByAvgProposerPriority()

    sort.Sort(ValidatorsByVotingPower(vals.Validators))

    return nil
#endif
  }

  void compute_new_priorities(std::vector<validator> updates, validator_set &vals, int64_t updated_total_voting_power) {
// computeNewPriorities computes the proposer priority for the validators not present in the set based on
// 'updatedTotalVotingPower'.
// Leaves unchanged the priorities of validators that are changed.
//
// 'updates' parameter must be a list of unique validators to be added or updated.
//
// 'updatedTotalVotingPower' is the total voting power of a set where all updates would be applied but
//   not the removals. It must be < 2*MaxTotalVotingPower and may be close to this limit if close to
//   MaxTotalVotingPower will be removed. This is still safe from overflow since MaxTotalVotingPower is maxInt64/8.
//
// No changes are made to the validator set 'vals'.
#if 0
    for _, valUpdate := range updates {
        address := valUpdate.Address
        _, val := vals.GetByAddress(address)
        if val == nil {
            // add val
            // Set ProposerPriority to -C*totalVotingPower (with C ~= 1.125) to make sure validators can't
            // un-bond and then re-bond to reset their (potentially previously negative) ProposerPriority to zero.
            //
            // Contract: updatedVotingPower < 2 * MaxTotalVotingPower to ensure ProposerPriority does
            // not exceed the bounds of int64.
            //
            // Compute ProposerPriority = -1.125*totalVotingPower == -(updatedVotingPower + (updatedVotingPower >> 3)).
            valUpdate.ProposerPriority = -(updatedTotalVotingPower + (updatedTotalVotingPower >> 3))
        } else {
            valUpdate.ProposerPriority = val.ProposerPriority
        }
    }
#endif
  }

  validator increment_proposer_priority(validator_set &vals) {
#if 0
    for _, val := range vals.Validators {
      // Check for overflow for sum.
      newPrio := safeAddClip(val.ProposerPriority, val.VotingPower)
      val.ProposerPriority = newPrio
    }
    // Decrement the validator with most ProposerPriority.
    mostest := vals.getValWithMostPriority()
    // Mind the underflow.
    mostest.ProposerPriority = safeSubClip(mostest.ProposerPriority, vals.TotalVotingPower())

    return mostest
#endif
  }

  int64_t rescale_priorities(validator_set &vals) {
// RescalePriorities rescales the priorities such that the distance between the
// maximum and minimum is smaller than `diffMax`. Panics if validator set is
// empty.
#if 0
    if vals.IsNilOrEmpty() {
      panic("empty validator set")
    }
    // NOTE: This check is merely a sanity check which could be
    // removed if all tests would init. voting power appropriately;
    // i.e. diffMax should always be > 0
    if diffMax <= 0 {
      return
    }

    // Calculating ceil(diff/diffMax):
    // Re-normalization is performed by dividing by an integer for simplicity.
    // NOTE: This may make debugging priority issues easier as well.
    diff := computeMaxMinPriorityDiff(vals)
    ratio := (diff + diffMax - 1) / diffMax
    if diff > diffMax {
          for _, val := range vals.Validators {
            val.ProposerPriority /= ratio
          }
      }
#endif
    return 0;
  }

  void shift_by_avg_proposer_priority(validator_set &vals) {
#if 0
    if vals.IsNilOrEmpty() {
      panic("empty validator set")
    }
    avgProposerPriority := vals.computeAvgProposerPriority()
    for _, val := range vals.Validators {
      val.ProposerPriority = safeSubClip(val.ProposerPriority, avgProposerPriority)
    }
#endif
  }

};

}
