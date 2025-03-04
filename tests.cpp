// tests.cpp
#include "crdt.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

// Helper function to generate unique IDs (simulating UUIDs)
CrdtString generate_uuid() {
  static uint64_t counter = 0;
  return "uuid-" + std::to_string(++counter);
}

/// Simple assertion helper
void assert_true(bool condition, const CrdtString &message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << std::endl;
    exit(1);
  }
}

int main() {
  // Test Case: Basic Insert and Merge using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Node1 inserts a record
    CrdtString record_id = generate_uuid();
    CrdtString form_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields1 = {{"id", record_id},
                                               {"form_id", form_id},
                                               {"tag", "Node1Tag"},
                                               {"created_at", "2023-10-01T12:00:00Z"},
                                               {"created_by", "User1"}};
    auto changes1 = node1.insert_or_update(record_id, std::move(fields1));

    // Node2 inserts the same record with different data
    CrdtMap<CrdtString, CrdtString> fields2 = {{"id", record_id},
                                               {"form_id", form_id},
                                               {"tag", "Node2Tag"},
                                               {"created_at", "2023-10-01T12:05:00Z"},
                                               {"created_by", "User2"}};
    auto changes2 = node2.insert_or_update(record_id, std::move(fields2));

    // Merge node2's changes into node1
    node1.merge_changes(std::move(changes2));

    // Merge node1's changes into node2
    node2.merge_changes(std::move(changes1));

    // Both nodes should resolve the conflict and have the same data
    assert_true(node1.get_data() == node2.get_data(), "Basic Insert and Merge: Data mismatch");
    assert_true(node1.get_data().at(record_id).fields.at("tag") == "Node2Tag",
                "Basic Insert and Merge: Tag should be 'Node2Tag'");
    assert_true(node1.get_data().at(record_id).fields.at("created_by") == "User2",
                "Basic Insert and Merge: created_by should be 'User2'");
    std::cout << "Test 'Basic Insert and Merge' passed." << std::endl;
  }

  // Test Case: Updates with Conflicts using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Insert a shared record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "InitialTag"}};
    auto changes_init1 = node1.insert_or_update(record_id, std::move(fields));
    auto changes_init2 = node2.insert_or_update(record_id, std::move(fields));

    // Merge initial inserts
    node1.merge_changes(std::move(changes_init2));
    node2.merge_changes(std::move(changes_init1));

    // Node1 updates 'tag'
    CrdtMap<CrdtString, CrdtString> updates1 = {{"tag", "Node1UpdatedTag"}};
    auto change_update1 = node1.insert_or_update(record_id, std::move(updates1));

    // Node2 updates 'tag'
    CrdtMap<CrdtString, CrdtString> updates2 = {{"tag", "Node2UpdatedTag"}};
    auto change_update2 = node2.insert_or_update(record_id, std::move(updates2));

    // Merge changes
    node1.merge_changes(std::move(change_update2));
    node2.merge_changes(std::move(change_update1));

    // Conflict resolved based on site_id (Node2 has higher site_id)
    assert_true(node1.get_data().at(record_id).fields.at("tag") == "Node2UpdatedTag",
                "Updates with Conflicts: Tag resolution mismatch");
    assert_true(node1.get_data() == node2.get_data(), "Updates with Conflicts: Data mismatch");
    std::cout << "Test 'Updates with Conflicts' passed." << std::endl;
  }

  // Test Case: Delete and Merge using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Insert and sync a record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "ToBeDeleted"}};
    auto changes_init = node1.insert_or_update(record_id, std::move(fields));

    // Merge to node2
    node2.merge_changes(std::move(changes_init));

    // Node1 deletes the record
    auto changes_delete = node1.delete_record(record_id);

    // Merge the deletion to node2
    node2.merge_changes(std::move(changes_delete));

    // Both nodes should reflect the deletion
    assert_true(node1.get_data().at(record_id).fields.empty(), "Delete and Merge: Node1 should have empty fields");
    assert_true(node2.get_data().at(record_id).fields.empty(), "Delete and Merge: Node2 should have empty fields");
    assert_true(node1.get_data().at(record_id).column_versions.find("__deleted__") !=
                    node1.get_data().at(record_id).column_versions.end(),
                "Delete and Merge: Node1 should have '__deleted__' column version");
    assert_true(node2.get_data().at(record_id).column_versions.find("__deleted__") !=
                    node2.get_data().at(record_id).column_versions.end(),
                "Delete and Merge: Node2 should have '__deleted__' column version");
    std::cout << "Test 'Delete and Merge' passed." << std::endl;
  }

  // Test Case: Tombstone Handling using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Insert a record and delete it on node1
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "Temporary"}};
    auto changes_insert = node1.insert_or_update(record_id, std::move(fields));
    auto changes_delete = node1.delete_record(record_id);

    // Merge changes to node2
    node2.merge_changes(std::move(changes_insert));
    node2.merge_changes(std::move(changes_delete));

    // Node2 tries to insert the same record
    auto changes_attempt_insert = node2.insert_or_update(record_id, std::move(fields));

    // Merge changes back to node1
    node1.merge_changes(std::move(changes_attempt_insert));

    // Node2 should respect the tombstone
    assert_true(node2.get_data().at(record_id).fields.empty(), "Tombstone Handling: Node2 should have empty fields");
    assert_true(node2.get_data().at(record_id).column_versions.find("__deleted__") !=
                    node2.get_data().at(record_id).column_versions.end(),
                "Tombstone Handling: Node2 should have '__deleted__' column version");
    std::cout << "Test 'Tombstone Handling' passed." << std::endl;
  }

  // Test Case: Conflict Resolution with site_id using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Both nodes insert a record with the same id
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields1 = {{"id", record_id}, {"tag", "Node1Tag"}};
    CrdtMap<CrdtString, CrdtString> fields2 = {{"id", record_id}, {"tag", "Node2Tag"}};
    auto changes1 = node1.insert_or_update(record_id, std::move(fields1));
    auto changes2 = node2.insert_or_update(record_id, std::move(fields2));

    // Merge changes
    node1.merge_changes(std::move(changes2));
    node2.merge_changes(std::move(changes1));

    // Both nodes update the 'tag' field multiple times
    CrdtMap<CrdtString, CrdtString> updates1 = {{"tag", "Node1Tag1"}};
    auto changes_update1 = node1.insert_or_update(record_id, std::move(updates1));

    updates1 = {{"tag", "Node1Tag2"}};
    auto changes_update2 = node1.insert_or_update(record_id, std::move(updates1));

    CrdtMap<CrdtString, CrdtString> updates2 = {{"tag", "Node2Tag1"}};
    auto changes_update3 = node2.insert_or_update(record_id, std::move(updates2));

    updates2 = {{"tag", "Node2Tag2"}};
    auto changes_update4 = node2.insert_or_update(record_id, std::move(updates2));

    // Merge changes
    node1.merge_changes(std::move(changes_update4));
    node2.merge_changes(std::move(changes_update2));
    node2.merge_changes(std::move(changes_update1));
    node1.merge_changes(std::move(changes_update3));

    // Since node2 has a higher site_id, its latest update should prevail
    CrdtString expected_tag = "Node2Tag2";

    assert_true(node1.get_data().at(record_id).fields.at("tag") == expected_tag, "Conflict Resolution: Tag resolution mismatch");
    assert_true(node1.get_data() == node2.get_data(), "Conflict Resolution: Data mismatch");
    std::cout << "Test 'Conflict Resolution with site_id' passed." << std::endl;
  }

  // Test Case: Logical Clock Update using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Node1 inserts a record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "Node1Tag"}};
    auto changes_insert = node1.insert_or_update(record_id, std::move(fields));

    // Node2 receives the change
    node2.merge_changes(std::move(changes_insert));

    // Node2's clock should be updated
    assert_true(node2.get_clock().current_time() > 0, "Logical Clock Update: Node2 clock should be greater than 0");
    assert_true(node2.get_clock().current_time() >= node1.get_clock().current_time(),
                "Logical Clock Update: Node2 clock should be >= Node1 clock");
    std::cout << "Test 'Logical Clock Update' passed." << std::endl;
  }

  // Test Case: Merge without Conflicts using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Node1 inserts a record
    CrdtString record_id1 = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields1 = {{"id", record_id1}, {"tag", "Node1Record"}};
    auto changes1 = node1.insert_or_update(record_id1, std::move(fields1));

    // Node2 inserts a different record
    CrdtString record_id2 = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields2 = {{"id", record_id2}, {"tag", "Node2Record"}};
    auto changes2 = node2.insert_or_update(record_id2, std::move(fields2));

    // Merge changes
    node1.merge_changes(std::move(changes2));
    node2.merge_changes(std::move(changes1));

    // Both nodes should have both records
    assert_true(node1.get_data().find(record_id1) != node1.get_data().end(),
                "Merge without Conflicts: Node1 should contain record_id1");
    assert_true(node1.get_data().find(record_id2) != node1.get_data().end(),
                "Merge without Conflicts: Node1 should contain record_id2");
    assert_true(node2.get_data().find(record_id1) != node2.get_data().end(),
                "Merge without Conflicts: Node2 should contain record_id1");
    assert_true(node2.get_data().find(record_id2) != node2.get_data().end(),
                "Merge without Conflicts: Node2 should contain record_id2");
    assert_true(node1.get_data() == node2.get_data(), "Merge without Conflicts: Data mismatch between Node1 and Node2");
    std::cout << "Test 'Merge without Conflicts' passed." << std::endl;
  }

  // Test Case: Multiple Merges using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Node1 inserts a record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "InitialTag"}};
    auto changes_init = node1.insert_or_update(record_id, std::move(fields));

    // Merge to node2
    node2.merge_changes(std::move(changes_init));

    // Node2 updates the record
    CrdtMap<CrdtString, CrdtString> updates2 = {{"tag", "UpdatedByNode2"}};
    auto changes_update2 = node2.insert_or_update(record_id, std::move(updates2));

    // Node1 updates the record
    CrdtMap<CrdtString, CrdtString> updates1 = {{"tag", "UpdatedByNode1"}};
    auto changes_update1 = node1.insert_or_update(record_id, std::move(updates1));

    // Merge changes
    node1.merge_changes(std::move(changes_update2));
    node2.merge_changes(std::move(changes_update1));

    // Since node2 has a higher site_id, its latest update should prevail
    CrdtString expected_tag = "UpdatedByNode2";

    assert_true(node1.get_data().at(record_id).fields.at("tag") == expected_tag, "Multiple Merges: Tag resolution mismatch");
    assert_true(node1.get_data() == node2.get_data(), "Multiple Merges: Data mismatch between Node1 and Node2");
    std::cout << "Test 'Multiple Merges' passed." << std::endl;
  }

  // Test Case: Inserting After Deletion using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Node1 inserts and deletes a record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "Temporary"}};
    auto changes_insert = node1.insert_or_update(record_id, std::move(fields));
    auto changes_delete = node1.delete_record(record_id);

    // Merge deletion to node2
    node2.merge_changes(std::move(changes_insert));
    node2.merge_changes(std::move(changes_delete));

    // Node2 tries to insert the same record
    auto changes_attempt_insert = node2.insert_or_update(record_id, std::move(fields));

    // Merge changes back to node1
    node1.merge_changes(std::move(changes_attempt_insert));

    // The deletion should prevail
    assert_true(node1.get_data().at(record_id).fields.empty(), "Inserting After Deletion: Node1 should have empty fields");
    assert_true(node2.get_data().at(record_id).fields.empty(), "Inserting After Deletion: Node2 should have empty fields");
    assert_true(node1.get_data().at(record_id).column_versions.find("__deleted__") !=
                    node1.get_data().at(record_id).column_versions.end(),
                "Inserting After Deletion: Node1 should have '__deleted__' column version");
    assert_true(node2.get_data().at(record_id).column_versions.find("__deleted__") !=
                    node2.get_data().at(record_id).column_versions.end(),
                "Inserting After Deletion: Node2 should have '__deleted__' column version");
    std::cout << "Test 'Inserting After Deletion' passed." << std::endl;
  }

  // Test Case: Offline Changes Then Merge using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Both nodes start with an empty state

    // Node1 inserts a record
    CrdtString record_id1 = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields1 = {{"id", record_id1}, {"tag", "Node1Tag"}};
    auto changes1 = node1.insert_or_update(record_id1, std::move(fields1));

    // Node2 is offline and inserts a different record
    CrdtString record_id2 = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields2 = {{"id", record_id2}, {"tag", "Node2Tag"}};
    auto changes2 = node2.insert_or_update(record_id2, std::move(fields2));

    // Now, node2 comes online and merges changes from node1
    uint64_t last_db_version_node2 = 0;
    sync_nodes(node1, node2, last_db_version_node2);

    // Similarly, node1 merges changes from node2
    uint64_t last_db_version_node1 = 0;
    sync_nodes(node2, node1, last_db_version_node1);

    // Both nodes should now have both records
    assert_true(node1.get_data().find(record_id1) != node1.get_data().end(),
                "Offline Changes Then Merge: Node1 should contain record_id1");
    assert_true(node1.get_data().find(record_id2) != node1.get_data().end(),
                "Offline Changes Then Merge: Node1 should contain record_id2");
    assert_true(node2.get_data().find(record_id1) != node2.get_data().end(),
                "Offline Changes Then Merge: Node2 should contain record_id1");
    assert_true(node2.get_data().find(record_id2) != node2.get_data().end(),
                "Offline Changes Then Merge: Node2 should contain record_id2");
    assert_true(node1.get_data() == node2.get_data(), "Offline Changes Then Merge: Data mismatch between Node1 and Node2");
    std::cout << "Test 'Offline Changes Then Merge' passed." << std::endl;
  }

  // Test Case: Conflicting Updates with Different Last DB Versions using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Both nodes insert the same record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields1 = {{"id", record_id}, {"tag", "InitialTag"}};
    CrdtMap<CrdtString, CrdtString> fields2 = {{"id", record_id}, {"tag", "InitialTag"}};
    auto changes_init1 = node1.insert_or_update(record_id, std::move(fields1));
    auto changes_init2 = node2.insert_or_update(record_id, std::move(fields2));

    // Merge initial inserts
    node1.merge_changes(std::move(changes_init2));
    node2.merge_changes(std::move(changes_init1));

    // Node1 updates 'tag' twice
    CrdtMap<CrdtString, CrdtString> updates_node1 = {{"tag", "Node1Tag1"}};
    auto changes_node1_update1 = node1.insert_or_update(record_id, std::move(updates_node1));

    updates_node1 = {{"tag", "Node1Tag2"}};
    auto changes_node1_update2 = node1.insert_or_update(record_id, std::move(updates_node1));

    // Node2 updates 'tag' once
    CrdtMap<CrdtString, CrdtString> updates_node2 = {{"tag", "Node2Tag1"}};
    auto changes_node2_update1 = node2.insert_or_update(record_id, std::move(updates_node2));

    // Merge node1's changes into node2
    node2.merge_changes(std::move(changes_node1_update1));
    node2.merge_changes(std::move(changes_node1_update2));

    // Merge node2's changes into node1
    node1.merge_changes(std::move(changes_node2_update1));

    // The 'tag' should reflect the latest update based on db_version and site_id Assuming node1 has a higher db_version due to
    // two updates
    CrdtString final_tag = "Node1Tag2";

    assert_true(node1.get_data().at(record_id).fields.at("tag") == final_tag,
                "Conflicting Updates: Final tag should be 'Node1Tag2'");
    assert_true(node2.get_data().at(record_id).fields.at("tag") == final_tag,
                "Conflicting Updates: Final tag should be 'Node1Tag2'");
    assert_true(node1.get_data() == node2.get_data(), "Conflicting Updates: Data mismatch between Node1 and Node2");
    std::cout << "Test 'Conflicting Updates with Different Last DB Versions' passed." << std::endl;
  }

  // // Test Case: Clock Synchronization After Merges using insert_or_update
  // {
  //   CRDT<CrdtString, CrdtString> node1(1);
  //   CRDT<CrdtString, CrdtString> node2(2);
  //   CRDT<CrdtString, CrdtString> node3(3);

  //   // Merge trackers
  //   uint64_t last_db_version_node1 = 0;
  //   uint64_t last_db_version_node2 = 0;
  //   uint64_t last_db_version_node3 = 0;

  //   // Node1 inserts a record
  //   CrdtString record_id1 = generate_uuid();
  //   CrdtMap<CrdtString, CrdtString> fields1 = {{"id", record_id1}, {"tag", "Node1Tag"}};
  //   auto changes1 = node1.insert_or_update(record_id1, std::move(fields1));

  //   // Node2 inserts another record
  //   CrdtString record_id2 = generate_uuid();
  //   CrdtMap<CrdtString, CrdtString> fields2 = {{"id", record_id2}, {"tag", "Node2Tag"}};
  //   auto changes2 = node2.insert_or_update(record_id2, std::move(fields2));

  //   // Node3 inserts a third record
  //   CrdtString record_id3 = generate_uuid();
  //   CrdtMap<CrdtString, CrdtString> fields3 = {{"id", record_id3}, {"tag", "Node3Tag"}};
  //   auto changes3 = node3.insert_or_update(record_id3, std::move(fields3));

  //   // First round of merges
  //   // Merge node1's changes into node2 and node3
  //   sync_nodes(node1, node2, last_db_version_node2);
  //   sync_nodes(node1, node3, last_db_version_node3);

  //   // Merge node2's changes into node1 and node3
  //   sync_nodes(node2, node1, last_db_version_node1);
  //   sync_nodes(node2, node3, last_db_version_node3);

  //   // Merge node3's changes into node1 and node2
  //   sync_nodes(node3, node1, last_db_version_node1);
  //   sync_nodes(node3, node2, last_db_version_node2);

  //   // All nodes should have all three records
  //   assert_true(node1.get_data() == node2.get_data(), "Clock Synchronization: Node1 and Node2 data mismatch");
  //   assert_true(node2.get_data() == node3.get_data(), "Clock Synchronization: Node2 and Node3 data mismatch");
  //   assert_true(node1.get_data() == node3.get_data(), "Clock Synchronization: Node1 and Node3 data mismatch");

  //   // Check that logical clocks are properly updated
  //   uint64_t min_expected_clock_value = 3; // At least 3 inserts happened
  //   assert_true(node1.get_clock().current_time() >= min_expected_clock_value, "Clock Synchronization: Node1 clock too low");
  //   assert_true(node2.get_clock().current_time() >= min_expected_clock_value, "Clock Synchronization: Node2 clock too low");
  //   assert_true(node3.get_clock().current_time() >= min_expected_clock_value, "Clock Synchronization: Node3 clock too low");

  //   // Capture max clock before another round of merges
  //   uint64_t max_clock_before_merge =
  //       std::max({node1.get_clock().current_time(), node2.get_clock().current_time(), node3.get_clock().current_time()});

  //   // Perform another round of merges
  //   sync_nodes(node1, node2, last_db_version_node2);
  //   sync_nodes(node2, node3, last_db_version_node3);
  //   sync_nodes(node3, node1, last_db_version_node1);

  //   // Check that clocks have been updated after merges
  //   assert_true(node1.get_clock().current_time() > max_clock_before_merge, "Clock Synchronization: Node1 clock did not
  //   update"); assert_true(node2.get_clock().current_time() > max_clock_before_merge, "Clock Synchronization: Node2 clock did
  //   not update"); assert_true(node3.get_clock().current_time() > max_clock_before_merge, "Clock Synchronization: Node3 clock
  //   did not update");

  //   // Since clocks don't need to be identical, we don't assert equality
  //   std::cout << "Test 'Clock Synchronization After Merges' passed." << std::endl;
  // }

  // Test Case: Atomic Sync Per Transaction using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Node1 inserts a record
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "InitialTag"}};
    auto changes_node1 = node1.insert_or_update(record_id, std::move(fields));

    // Sync immediately after the transaction
    node2.merge_changes(std::move(changes_node1));

    // Verify synchronization
    assert_true(node2.get_data().find(record_id) != node2.get_data().end(),
                "Atomic Sync: Node2 should contain the inserted record");
    assert_true(node2.get_data().at(record_id).fields.at("tag") == "InitialTag", "Atomic Sync: Tag should be 'InitialTag'");
    std::cout << "Test 'Atomic Sync Per Transaction' passed." << std::endl;
  }

  // Test Case: Concurrent Updates using insert_or_update
  {
    CRDT<CrdtString, CrdtString> node1(1);
    CRDT<CrdtString, CrdtString> node2(2);

    // Insert a record on node1
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"tag", "InitialTag"}};
    auto changes_insert = node1.insert_or_update(record_id, std::move(fields));

    // Merge to node2
    node2.merge_changes(std::move(changes_insert));

    // Concurrently update 'tag' on both nodes
    CrdtMap<CrdtString, CrdtString> updates_node1 = {{"tag", "Node1TagUpdate"}};
    auto changes_update1 = node1.insert_or_update(record_id, std::move(updates_node1));

    CrdtMap<CrdtString, CrdtString> updates_node2 = {{"tag", "Node2TagUpdate"}};
    auto changes_update2 = node2.insert_or_update(record_id, std::move(updates_node2));

    // Merge changes
    node1.merge_changes(std::move(changes_update2));
    node2.merge_changes(std::move(changes_update1));

    // Conflict resolution based on site_id (Node2 has higher site_id)
    CrdtString expected_tag = "Node2TagUpdate";

    assert_true(node1.get_data().at(record_id).fields.at("tag") == expected_tag,
                "Concurrent Updates: Tag should be 'Node2TagUpdate'");
    assert_true(node2.get_data().at(record_id).fields.at("tag") == expected_tag,
                "Concurrent Updates: Tag should be 'Node2TagUpdate'");
    std::cout << "Test 'Concurrent Updates' passed." << std::endl;
  }

  // Test Case: Get Changes Since After Loading with Merge Versions
  {
    // Initialize CRDT with pre-loaded changes
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    CrdtNodeId node_id = 1;

    CrdtString record_id = generate_uuid();
    changes.emplace_back(Change<CrdtString, CrdtString>(record_id, "field1", "value1", 1, 1, node_id));
    CRDT<CrdtString, CrdtString> crdt_loaded(node_id, std::move(changes));

    // Make additional changes after loading
    CrdtMap<CrdtString, CrdtString> new_fields = {{"field2", "value2"}};
    auto changes_new = crdt_loaded.insert_or_update(record_id, std::move(new_fields));

    // Retrieve changes since db_version 1
    CrdtVector<Change<CrdtString, CrdtString>> retrieved_changes = crdt_loaded.get_changes_since(1);

    // Should include only the new change
    assert_true(retrieved_changes.size() == 1, "Get Changes Since: Should retrieve one new change");
    assert_true(retrieved_changes[0].col_name.has_value() && retrieved_changes[0].col_name.value() == "field2",
                "Get Changes Since: Retrieved change should be for 'field2'");
    assert_true(retrieved_changes[0].value.has_value() && retrieved_changes[0].value.value() == "value2",
                "Get Changes Since: Retrieved change 'field2' value mismatch");
    std::cout << "Test 'Get Changes Since After Loading with Merge Versions' passed." << std::endl;
  }

  // Test Case: Prevent Reapplication of Changes Loaded via Constructor
  {
    // Initialize CRDT with pre-loaded changes
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    CrdtNodeId node_id = 1;

    CrdtString record_id = generate_uuid();
    changes.emplace_back(Change<CrdtString, CrdtString>(record_id, "field1", "value1", 1, 1, node_id));
    CRDT<CrdtString, CrdtString> crdt_loaded(node_id, std::move(changes));

    // Attempt to merge the same changes again
    crdt_loaded.merge_changes({Change<CrdtString, CrdtString>(record_id, "field1", "value1", 1, 1, node_id)});

    // Verify that no duplicate changes are applied
    const auto &data = crdt_loaded.get_data();
    assert_true(data.at(record_id).fields.at("field1") == "value1",
                "Prevent Reapplication: 'field1' value should remain 'value1'");
    std::cout << "Test 'Prevent Reapplication of Changes Loaded via Constructor' passed." << std::endl;
  }

  // Test Case: Complex Merge Scenario with Merge DB Versions
  {
    // Initialize two CRDTs with pre-loaded changes
    CrdtVector<Change<CrdtString, CrdtString>> changes_node1;
    CrdtNodeId node1_id = 1;

    CrdtString record_id = generate_uuid();
    changes_node1.emplace_back(Change<CrdtString, CrdtString>(record_id, "field1", "node1_value1", 1, 1, node1_id));
    CRDT<CrdtString, CrdtString> node1_crdt(node1_id, std::move(changes_node1));

    CrdtVector<Change<CrdtString, CrdtString>> changes_node2;
    CrdtNodeId node2_id = 2;
    changes_node2.emplace_back(Change<CrdtString, CrdtString>(record_id, "field1", "node2_value1", 2, 2, node2_id));
    CRDT<CrdtString, CrdtString> node2_crdt(node2_id, std::move(changes_node2));

    // Merge node2 into node1
    node1_crdt.merge_changes({Change<CrdtString, CrdtString>(record_id, "field1", "node2_value1", 2, 2, node2_id)});

    // Merge node1 into node2
    node2_crdt.merge_changes({Change<CrdtString, CrdtString>(record_id, "field1", "node1_value1", 1, 1, node1_id)});

    // Verify conflict resolution based on db_version and node_id
    // node2's change should prevail since it has a higher db_version
    assert_true(node1_crdt.get_data().at(record_id).fields.at("field1") == "node2_value1",
                "Complex Merge: node2's change should prevail in node1");
    assert_true(node2_crdt.get_data().at(record_id).fields.at("field1") == "node2_value1",
                "Complex Merge: node2's change should prevail in node2");
    std::cout << "Test 'Complex Merge Scenario with Merge DB Versions' passed." << std::endl;
  }

  // Test Case: get_changes_since Considers merge_db_version Correctly
  {
    // Initialize CRDT and perform initial changes
    CRDT<CrdtString, CrdtString> crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"field1", "value1"}};
    auto changes_init = crdt.insert_or_update(record_id, std::move(fields));

    // Apply changes and set merge_db_version via constructor
    CRDT<CrdtString, CrdtString> crdt_loaded(2, std::move(changes_init));

    // Make new changes after loading
    CrdtMap<CrdtString, CrdtString> new_fields = {{"field2", "value2"}};
    auto changes_new = crdt_loaded.insert_or_update(record_id, std::move(new_fields));

    // Get changes since db_version 1
    CrdtVector<Change<CrdtString, CrdtString>> retrieved_changes = crdt_loaded.get_changes_since(1);

    // Should include only the new change
    assert_true(retrieved_changes.size() == 1, "get_changes_since: Should retrieve one new change");
    assert_true(retrieved_changes[0].col_name.has_value() && retrieved_changes[0].col_name.value() == "field2",
                "get_changes_since: Retrieved change should be for 'field2'");
    assert_true(retrieved_changes[0].value.has_value() && retrieved_changes[0].value.value() == "value2",
                "get_changes_since: Retrieved change 'field2' value mismatch");
    std::cout << "Test 'get_changes_since Considers merge_db_version Correctly' passed." << std::endl;
  }

  // Test Case: Multiple Loads and Merges with Merge DB Versions
  {
    // Simulate loading from disk multiple times
    CrdtVector<Change<CrdtString, CrdtString>> changes_load1;
    CrdtNodeId node_id = 1;

    CrdtString record_id1 = generate_uuid();
    changes_load1.emplace_back(Change<CrdtString, CrdtString>(record_id1, "field1", "value1", 1, 1, node_id));
    CRDT<CrdtString, CrdtString> crdt1(node_id, std::move(changes_load1));

    CrdtVector<Change<CrdtString, CrdtString>> changes_load2;
    CrdtString record_id2 = generate_uuid();
    changes_load2.emplace_back(Change<CrdtString, CrdtString>(record_id2, "field2", "value2", 2, 2, node_id));
    CRDT<CrdtString, CrdtString> crdt2(node_id, std::move(changes_load2));

    // Merge crdt2 into crdt1
    crdt1.merge_changes({Change<CrdtString, CrdtString>(record_id2, "field2", "value2", 2, 2, node_id)});

    // Make additional changes
    CrdtMap<CrdtString, CrdtString> new_fields = {{"field3", "value3"}};
    auto changes_new = crdt1.insert_or_update(record_id1, std::move(new_fields));

    // Get changes since db_version 3
    CrdtVector<Change<CrdtString, CrdtString>> retrieved_changes = crdt1.get_changes_since(3);

    // Should include only the new change
    assert_true(retrieved_changes.size() == 1, "Multiple Loads and Merges: Should retrieve one new change");
    assert_true(retrieved_changes[0].col_name.has_value() && retrieved_changes[0].col_name.value() == "field3",
                "Multiple Loads and Merges: Retrieved change should be for 'field3'");
    assert_true(retrieved_changes[0].value.has_value() && retrieved_changes[0].value.value() == "value3",
                "Multiple Loads and Merges: Retrieved change 'field3' value mismatch");
    std::cout << "Test 'Multiple Loads and Merges with Merge DB Versions' passed." << std::endl;
  }

  // Test Case: Parent-Child Overlay Functionality
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id_parent = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id_parent}, {"parent_field", "parent_value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id_parent, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Child should inherit parent's record
    assert_true(child_crdt.get_data().find(record_id_parent) != child_crdt.get_data().end(),
                "Parent-Child Overlay: Child should inherit parent's record");
    assert_true(child_crdt.get_data().at(record_id_parent).fields.at("parent_field") == "parent_value",
                "Parent-Child Overlay: Inherited field value mismatch");

    // Child updates the inherited record
    CrdtMap<CrdtString, CrdtString> child_updates = {{"child_field", "child_value"}};
    auto child_changes = child_crdt.insert_or_update(record_id_parent, std::move(child_updates));

    // Merge child's changes back to parent
    parent_crdt.merge_changes(std::move(child_changes));

    // Parent should now have the child's field
    assert_true(parent_crdt.get_data().at(record_id_parent).fields.at("child_field") == "child_value",
                "Parent-Child Overlay: Parent should reflect child's update");

    std::cout << "Test 'Parent-Child Overlay Functionality' passed." << std::endl;
  }

  // Add these tests after the existing tests in the main function

  // Test Case: Parent-Child Overlay with Multiple Levels
  {
    // Create grandparent CRDT
    CRDT<CrdtString, CrdtString> grandparent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> grandparent_fields = {{"id", record_id}, {"level", "grandparent"}};
    grandparent_crdt.insert_or_update(record_id, std::move(grandparent_fields));

    // Create parent CRDT with grandparent
    auto grandparent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(grandparent_crdt);
    CRDT<CrdtString, CrdtString> parent_crdt(2, grandparent_ptr);
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"level", "parent"}};
    parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(3, parent_ptr);
    CrdtMap<CrdtString, CrdtString> child_fields = {{"level", "child"}};
    child_crdt.insert_or_update(record_id, std::move(child_fields));

    // Check that child has the most recent value
    assert_true(child_crdt.get_data().at(record_id).fields.at("level") == "child",
                "Multi-level Overlay: Child should have its own value");

    // Check that parent has its own value
    assert_true(parent_crdt.get_data().at(record_id).fields.at("level") == "parent",
                "Multi-level Overlay: Parent should have its own value");

    // Check that grandparent has its original value
    assert_true(grandparent_crdt.get_data().at(record_id).fields.at("level") == "grandparent",
                "Multi-level Overlay: Grandparent should have its original value");

    std::cout << "Test 'Parent-Child Overlay with Multiple Levels' passed." << std::endl;
  }

  // Test Case: Inheritance of Records from Parent
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id1 = generate_uuid();
    CrdtString record_id2 = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields1 = {{"id", record_id1}, {"data", "parent_data1"}};
    CrdtMap<CrdtString, CrdtString> parent_fields2 = {{"id", record_id2}, {"data", "parent_data2"}};
    parent_crdt.insert_or_update(record_id1, std::move(parent_fields1));
    parent_crdt.insert_or_update(record_id2, std::move(parent_fields2));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Check that child inherits both records from parent
    assert_true(child_crdt.get_data().at(record_id1).fields.at("data") == "parent_data1",
                "Record Inheritance: Child should inherit record1 from parent");
    assert_true(child_crdt.get_data().at(record_id2).fields.at("data") == "parent_data2",
                "Record Inheritance: Child should inherit record2 from parent");

    std::cout << "Test 'Inheritance of Records from Parent' passed." << std::endl;
  }

  // Test Case: Overriding Parent Records in Child
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id}, {"data", "parent_data"}};
    parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Override parent's record in child
    CrdtMap<CrdtString, CrdtString> child_fields = {{"data", "child_data"}};
    child_crdt.insert_or_update(record_id, std::move(child_fields));

    // Check that child has its own value
    assert_true(child_crdt.get_data().at(record_id).fields.at("data") == "child_data",
                "Record Override: Child should have its own value");

    // Check that parent still has its original value
    assert_true(parent_crdt.get_data().at(record_id).fields.at("data") == "parent_data",
                "Record Override: Parent should retain its original value");

    std::cout << "Test 'Overriding Parent Records in Child' passed." << std::endl;
  }

  // Test Case: Merging Changes from Child to Parent
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id}, {"parent_field", "parent_value"}};
    parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Child adds a new field
    CrdtMap<CrdtString, CrdtString> child_fields = {{"child_field", "child_value"}};
    auto child_changes = child_crdt.insert_or_update(record_id, std::move(child_fields));

    // Merge child's changes to parent
    parent_crdt.merge_changes(std::move(child_changes));

    // Check that parent now has the child's field
    assert_true(parent_crdt.get_data().at(record_id).fields.at("child_field") == "child_value",
                "Child to Parent Merge: Parent should have child's new field");

    // Check that parent retains its original field
    assert_true(parent_crdt.get_data().at(record_id).fields.at("parent_field") == "parent_value",
                "Child to Parent Merge: Parent should retain its original field");

    std::cout << "Test 'Merging Changes from Child to Parent' passed." << std::endl;
  }

  // Test Case: Get Changes Since with Parent-Child Relationship
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id}, {"parent_field", "parent_value"}};
    parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Child adds a new field
    CrdtMap<CrdtString, CrdtString> child_fields = {{"child_field", "child_value"}};
    child_crdt.insert_or_update(record_id, std::move(child_fields));

    // Get changes since the beginning
    auto changes = child_crdt.get_changes_since(0);

    // Check that changes include both parent and child fields
    bool has_parent_field = false;
    bool has_child_field = false;
    for (const auto &change : changes) {
      if (change.col_name == "parent_field" && change.value == "parent_value") {
        has_parent_field = true;
      }
      if (change.col_name == "child_field" && change.value == "child_value") {
        has_child_field = true;
      }
    }

    assert_true(has_parent_field, "Get Changes Since: Should include parent's field");
    assert_true(has_child_field, "Get Changes Since: Should include child's field");

    std::cout << "Test 'Get Changes Since with Parent-Child Relationship' passed." << std::endl;
  }

  // Test Case: Tombstone Propagation from Parent to Child
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"field", "value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id, std::move(fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Child should inherit the record
    assert_true(child_crdt.get_data().find(record_id) != child_crdt.get_data().end(),
                "Tombstone Propagation: Child should inherit the record from parent");

    // Parent deletes the record
    auto parent_delete_changes = parent_crdt.delete_record(record_id);

    // Merge deletion into child
    child_crdt.merge_changes(std::move(parent_delete_changes), true);

    // Child should now have the record tombstoned
    assert_true(child_crdt.get_data().at(record_id).fields.empty(),
                "Tombstone Propagation: Child should have empty fields after deletion");
    assert_true(child_crdt.get_data().at(record_id).column_versions.find("__deleted__") !=
                    child_crdt.get_data().at(record_id).column_versions.end(),
                "Tombstone Propagation: Child should have '__deleted__' column version");

    std::cout << "Test 'Tombstone Propagation from Parent to Child' passed." << std::endl;
  }

  // Test Case: Conflict Resolution with Parent and Child CRDTs
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id}, {"field", "parent_value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Both parent and child update the same field concurrently
    CrdtMap<CrdtString, CrdtString> parent_update = {{"field", "parent_updated"}};
    auto parent_change_update = parent_crdt.insert_or_update(record_id, std::move(parent_update));

    CrdtMap<CrdtString, CrdtString> child_update = {{"field", "child_updated"}};
    auto child_change_update = child_crdt.insert_or_update(record_id, std::move(child_update));

    // Merge child's changes into parent
    parent_crdt.merge_changes(std::move(child_change_update), true);

    // Merge parent's changes into child
    child_crdt.merge_changes(std::move(parent_change_update), true);

    // Conflict resolution should prefer the change with the higher db_version or higher node_id
    // Assuming parent and child have different db_versions, the resolution will follow the rules
    // Let's verify which update prevailed

    // Fetch the final value from both parent and child
    std::string parent_final = parent_crdt.get_data().at(record_id).fields.at("field");
    std::string child_final = child_crdt.get_data().at(record_id).fields.at("field");

    // Both should be the same
    assert_true(parent_final == child_final, "Conflict Resolution with Parent and Child: Data mismatch between parent and child");

    // Depending on the db_version and node_id, determine the expected value
    // Since child has a higher node_id, if db_versions are equal, child's update should prevail
    // Otherwise, the higher db_version determines the winner

    // For simplicity, let's assume child had a higher db_version
    // Thus, expected value should be "child_updated"
    std::string expected = "child_updated";
    assert_true(parent_final == expected, "Conflict Resolution with Parent and Child: Expected 'child_updated'");

    std::cout << "Test 'Conflict Resolution with Parent and Child CRDTs' passed." << std::endl;
  }

  // Test Case: Hierarchical Change Retrieval
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id_parent = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id_parent}, {"parent_field", "parent_value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id_parent, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Child adds its own record
    CrdtString record_id_child = generate_uuid();
    CrdtMap<CrdtString, CrdtString> child_fields = {{"id", record_id_child}, {"child_field", "child_value"}};
    auto child_changes = child_crdt.insert_or_update(record_id_child, std::move(child_fields));

    // Retrieve changes since db_version 0 from child
    CrdtVector<Change<CrdtString, CrdtString>> retrieved_changes = child_crdt.get_changes_since(0);

    // Should include both parent and child changes
    assert_true(retrieved_changes.size() == 4, "Hierarchical Change Retrieval: Should retrieve four changes");

    // Verify that both changes are present
    bool parent_change_found = false;
    bool child_change_found = false;
    for (const auto &change : retrieved_changes) {
      if (change.record_id == record_id_parent && change.col_name.has_value() && change.col_name.value() == "parent_field" &&
          change.value.has_value() && change.value.value() == "parent_value") {
        parent_change_found = true;
      }
      if (change.record_id == record_id_child && change.col_name.has_value() && change.col_name.value() == "child_field" &&
          change.value.has_value() && change.value.value() == "child_value") {
        child_change_found = true;
      }
    }
    assert_true(parent_change_found, "Hierarchical Change Retrieval: Parent change not found");
    assert_true(child_change_found, "Hierarchical Change Retrieval: Child change not found");

    std::cout << "Test 'Hierarchical Change Retrieval' passed." << std::endl;
  }

  // Test Case: Avoiding Duplicate Change Application via Parent
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id}, {"field", "parent_value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Parent inserts a new field
    CrdtMap<CrdtString, CrdtString> parent_new_field = {{"new_field", "new_parent_value"}};
    auto parent_change_new_field = parent_crdt.insert_or_update(record_id, std::move(parent_new_field));

    // Merge parent's new field into child
    child_crdt.merge_changes(std::move(parent_change_new_field));

    // Attempt to re-merge the same change into child
    child_crdt.merge_changes({Change<CrdtString, CrdtString>(record_id, "new_field", "new_parent_value", 2, 2, 1)});

    // Verify that 'new_field' is correctly set without duplication
    assert_true(child_crdt.get_data().at(record_id).fields.at("new_field") == "new_parent_value",
                "Avoiding Duplicate Changes: 'new_field' value mismatch");

    std::cout << "Test 'Avoiding Duplicate Change Application via Parent' passed." << std::endl;
  }

  // Test Case: Child Deletion Does Not Affect Parent
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id}, {"field", "parent_value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id, std::move(parent_fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Child deletes the record
    auto child_delete_changes = child_crdt.delete_record(record_id);

    // Merge child's deletion into parent
    parent_crdt.merge_changes(std::move(child_delete_changes));

    // Parent should still have the record (since child deletion should not affect parent)
    assert_true(parent_crdt.get_data().find(record_id) != parent_crdt.get_data().end(),
                "Child Deletion: Parent should still have the record after child deletion");

    // Child should have the record tombstoned
    assert_true(child_crdt.get_data().at(record_id).fields.empty(),
                "Child Deletion: Child should have empty fields after deletion");
    assert_true(child_crdt.get_data().at(record_id).column_versions.find("__deleted__") !=
                    child_crdt.get_data().at(record_id).column_versions.end(),
                "Child Deletion: Child should have '__deleted__' column version");

    std::cout << "Test 'Child Deletion Does Not Affect Parent' passed." << std::endl;
  }

  // Test Case: Parent and Child Simultaneous Updates
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"field1", "value1"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id, std::move(fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Parent updates field1
    CrdtMap<CrdtString, CrdtString> parent_update = {{"field1", "parent_updated"}};
    auto parent_change_update = parent_crdt.insert_or_update(record_id, std::move(parent_update));

    // Child updates field2
    CrdtMap<CrdtString, CrdtString> child_update = {{"field2", "child_value2"}};
    auto child_change_update = child_crdt.insert_or_update(record_id, std::move(child_update));

    // Merge changes
    parent_crdt.merge_changes(std::move(child_change_update));
    child_crdt.merge_changes(std::move(parent_change_update));

    // Verify that both updates are present
    assert_true(parent_crdt.get_data().at(record_id).fields.at("field1") == "parent_updated",
                "Simultaneous Updates: Parent's field1 should be updated");
    assert_true(parent_crdt.get_data().at(record_id).fields.at("field2") == "child_value2",
                "Simultaneous Updates: Parent should have child's field2");

    assert_true(child_crdt.get_data().at(record_id).fields.at("field1") == "parent_updated",
                "Simultaneous Updates: Child's field1 should reflect parent's update");
    assert_true(child_crdt.get_data().at(record_id).fields.at("field2") == "child_value2",
                "Simultaneous Updates: Child's field2 should be updated");

    std::cout << "Test 'Parent and Child Simultaneous Updates' passed." << std::endl;
  }

  // Test Case: Parent Deletion Prevents Child Insertions
  {
    // Create parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id = generate_uuid();
    CrdtMap<CrdtString, CrdtString> fields = {{"id", record_id}, {"field", "value"}};
    auto parent_changes = parent_crdt.insert_or_update(record_id, std::move(fields));

    // Create child CRDT with parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Parent deletes the record
    auto parent_delete_changes = parent_crdt.delete_record(record_id);

    // Merge deletion into child
    child_crdt.merge_changes(std::move(parent_delete_changes));

    // Child attempts to insert a new field into the tombstoned record
    CrdtMap<CrdtString, CrdtString> child_insert = {{"field2", "new_value"}};
    auto child_change_insert = child_crdt.insert_or_update(record_id, std::move(child_insert));

    // Merge child's insertion back into parent
    parent_crdt.merge_changes(std::move(child_change_insert));

    // Parent should still have the record tombstoned without the new field
    assert_true(parent_crdt.get_data().at(record_id).fields.empty(),
                "Parent Deletion: Parent should still have empty fields after child insertion attempt");
    assert_true(parent_crdt.get_data().at(record_id).column_versions.find("__deleted__") !=
                    parent_crdt.get_data().at(record_id).column_versions.end(),
                "Parent Deletion: Parent should have '__deleted__' column version");

    // Child should also respect the tombstone
    assert_true(child_crdt.get_data().at(record_id).fields.empty(),
                "Parent Deletion: Child should have empty fields after parent's deletion");
    assert_true(child_crdt.get_data().at(record_id).column_versions.find("__deleted__") !=
                    child_crdt.get_data().at(record_id).column_versions.end(),
                "Parent Deletion: Child should have '__deleted__' column version");

    std::cout << "Test 'Parent Deletion Prevents Child Insertions' passed." << std::endl;
  }

  // Test Case 1: Reverting a Child CRDT Restores Parent's State
  {
    // Step 1: Initialize Parent CRDT
    CRDT<CrdtString, CrdtString> parent_crdt(1);
    CrdtString record_id_parent = generate_uuid();
    CrdtMap<CrdtString, CrdtString> parent_fields = {{"id", record_id_parent}, {"parent_field", "parent_value"}};
    parent_crdt.insert_or_update(record_id_parent, std::move(parent_fields));

    // Step 2: Initialize Child CRDT with Parent
    auto parent_ptr = std::make_shared<CRDT<CrdtString, CrdtString>>(parent_crdt);
    CRDT<CrdtString, CrdtString> child_crdt(2, parent_ptr);

    // Step 3: Modify Child CRDT
    CrdtMap<CrdtString, CrdtString> child_fields = {{"child_field1", "child_value1"}, {"child_field2", "child_value2"}};
    child_crdt.insert_or_update(record_id_parent, std::move(child_fields));

    // Verify Child has additional fields
    assert_true(child_crdt.get_data().at(record_id_parent).fields.at("child_field1") == "child_value1",
                "Revert Test 1: Child should have 'child_field1' with 'child_value1'");
    assert_true(child_crdt.get_data().at(record_id_parent).fields.at("child_field2") == "child_value2",
                "Revert Test 1: Child should have 'child_field2' with 'child_value2'");

    // Step 4: Revert Child CRDT
    CrdtVector<Change<CrdtString, CrdtString>> inverse_changes = child_crdt.revert();

    //! Cannot work because inverse_changes is in a special format that cannot be simply merged back into the CRDT
    //! it is meant to be used by the application layer to revert changes, not by the CRDT itself for now

    // Apply inverse changes to child CRDT to undo modifications
    child_crdt.merge_changes(std::move(inverse_changes), true);

    // // Step 5: Validate States
    // // Child should now match the parent
    // assert_true(child_crdt.get_data().at(record_id_parent).fields == parent_crdt.get_data().at(record_id_parent).fields,
    //             "Revert Test 1: Child's fields should match parent's fields after revert");

    // // Parent remains unchanged
    // assert_true(parent_crdt.get_data().at(record_id_parent).fields.at("parent_field") == "parent_value",
    //             "Revert Test 1: Parent's 'parent_field' should remain 'parent_value'");

    // std::cout << "Test 'Reverting a Child CRDT Restores Parent's State' passed." << std::endl;
  }

  // Test Case 1: Compress with No Changes
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    CRDT<CrdtString, CrdtString>::compress_changes(changes);
    assert_true(changes.empty(), "Compress Changes: No changes should remain after compression.");
    std::cout << "Test 'Compress with No Changes' passed." << std::endl;
  }

  // Test Case 2: Single Change should remain unchanged
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "value1", 1, 1, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    assert_true(changes.size() == 1, "Compress Changes: Single change should remain unchanged.");
    assert_true(changes[0].record_id == "record1" && changes[0].col_name == "col1" && changes[0].value == "value1",
                "Compress Changes: Single change content mismatch.");
    std::cout << "Test 'Single Change Unchanged' passed." << std::endl;
  }

  // Test Case 3: Multiple Changes on Different Records and Columns
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "value1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col2", "value2", 1, 2, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record3", "col3", "value3", 1, 3, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    assert_true(changes.size() == 3, "Compress Changes: All distinct changes should remain.");
    std::cout << "Test 'Multiple Distinct Changes' passed." << std::endl;
  }

  // Test Case 4: Multiple Changes on the Same Record and Same Column
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Older change
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "old_value", 1, 1, 1));
    // Newer change
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "new_value", 2, 2, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    assert_true(changes.size() == 1, "Compress Changes: Only the latest change should remain.");
    assert_true(changes[0].value == "new_value", "Compress Changes: Latest change value mismatch.");
    std::cout << "Test 'Multiple Changes Same Record and Column' passed." << std::endl;
  }

  // Test Case 5: Multiple Changes on the Same Record but Different Columns
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "value1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "value2", 1, 2, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col3", "value3", 1, 3, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    assert_true(changes.size() == 3, "Compress Changes: Changes on different columns should remain.");
    std::cout << "Test 'Multiple Changes Same Record Different Columns' passed." << std::endl;
  }

  // Test Case 6: Interleaved Changes on Multiple Records and Columns
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Record1, Column1
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "v1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "v2", 2, 2, 1));
    // Record2, Column2
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col2", "v3", 1, 3, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col2", "v4", 2, 4, 1));
    // Record1, Column3
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col3", "v5", 1, 5, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, col1: "v2"
    // - record2, col2: "v4"
    // - record1, col3: "v5"
    assert_true(changes.size() == 3, "Compress Changes: Should compress to latest changes per column.");
    for (const auto &change : changes) {
      if (change.record_id == "record1" && change.col_name == "col1") {
        assert_true(change.value == "v2", "Compress Changes: record1 col1 value mismatch.");
      } else if (change.record_id == "record2" && change.col_name == "col2") {
        assert_true(change.value == "v4", "Compress Changes: record2 col2 value mismatch.");
      } else if (change.record_id == "record1" && change.col_name == "col3") {
        assert_true(change.value == "v5", "Compress Changes: record1 col3 value mismatch.");
      } else {
        assert_true(false, "Compress Changes: Unexpected change present.");
      }
    }
    std::cout << "Test 'Interleaved Changes on Multiple Records and Columns' passed." << std::endl;
  }

  // Test Case 7: Changes Including Deletions
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Insertions
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "value1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "value2", 1, 2, 1));
    // Update col1
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "value3", 2, 3, 1));
    // Delete col2
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", std::nullopt, 2, 4, 1));
    // Insert col3
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col3", "value4", 1, 5, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, col1: "value3"
    // - record1, col2: deletion (std::nullopt)
    // - record1, col3: "value4"
    assert_true(changes.size() == 3, "Compress Changes: Should compress updates and deletions correctly.");
    for (const auto &change : changes) {
      if (change.record_id == "record1" && change.col_name == "col1") {
        assert_true(change.value == "value3", "Compress Changes: record1 col1 latest value mismatch.");
      } else if (change.record_id == "record1" && change.col_name == "col2") {
        assert_true(!change.value.has_value(), "Compress Changes: record1 col2 should be deleted.");
      } else if (change.record_id == "record1" && change.col_name == "col3") {
        assert_true(change.value == "value4", "Compress Changes: record1 col3 value mismatch.");
      } else {
        assert_true(false, "Compress Changes: Unexpected change present.");
      }
    }
    std::cout << "Test 'Changes Including Deletions' passed." << std::endl;
  }

  // Test Case 8: Multiple Deletions on the Same Record
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // First deletion
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", std::nullopt, std::nullopt, 1, 1, 1));
    // Second deletion (redundant)
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", std::nullopt, std::nullopt, 2, 2, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, __deleted__: latest deletion
    assert_true(changes.size() == 1, "Compress Changes: Multiple deletions should compress to latest.");
    assert_true(!changes[0].col_name.has_value(), "Compress Changes: Deletion should have no column name.");
    assert_true(!changes[0].value.has_value(), "Compress Changes: Deletion should have no value.");
    assert_true(changes[0].col_version == 2, "Compress Changes: Latest deletion col_version mismatch.");
    std::cout << "Test 'Multiple Deletions on the Same Record' passed." << std::endl;
  }

  // Test Case 9: Mixed Inserts, Updates, and Deletions Across Multiple Records
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Record1
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v2", 2, 2, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "r1c2_v1", 1, 3, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", std::nullopt, 2, 4, 1)); // Deletion
    // Record2
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v1", 1, 5, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v2", 2, 6, 1));
    // Record3
    changes.emplace_back(Change<CrdtString, CrdtString>("record3", "col1", "r3c1_v1", 1, 7, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, col1: "r1c1_v2"
    // - record1, col2: deletion
    // - record2, col1: "r2c1_v2"
    // - record3, col1: "r3c1_v1"
    assert_true(changes.size() == 4, "Compress Changes: Mixed operations should compress correctly.");
    for (const auto &change : changes) {
      if (change.record_id == "record1" && change.col_name == "col1") {
        assert_true(change.value == "r1c1_v2", "Compress Changes: record1 col1 latest value mismatch.");
      } else if (change.record_id == "record1" && change.col_name == "col2") {
        assert_true(!change.value.has_value(), "Compress Changes: record1 col2 should be deleted.");
      } else if (change.record_id == "record2" && change.col_name == "col1") {
        assert_true(change.value == "r2c1_v2", "Compress Changes: record2 col1 latest value mismatch.");
      } else if (change.record_id == "record3" && change.col_name == "col1") {
        assert_true(change.value == "r3c1_v1", "Compress Changes: record3 col1 value mismatch.");
      } else {
        assert_true(false, "Compress Changes: Unexpected change present.");
      }
    }
    std::cout << "Test 'Mixed Inserts, Updates, and Deletions Across Multiple Records' passed." << std::endl;
  }

  // Test Case 10: Compression Order Verification
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Out-of-order changes
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v1", 1, 5, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v2", 2, 2, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v2", 2, 6, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "r1c2_v1", 1, 3, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "r1c2_v2", 2, 4, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, col1: "r1c1_v2"
    // - record1, col2: "r1c2_v2"
    // - record2, col1: "r2c1_v2"
    assert_true(changes.size() == 3, "Compress Changes: Compression should handle out-of-order changes correctly.");
    for (const auto &change : changes) {
      if (change.record_id == "record1" && change.col_name == "col1") {
        assert_true(change.value == "r1c1_v2", "Compress Changes: record1 col1 latest value mismatch.");
      } else if (change.record_id == "record1" && change.col_name == "col2") {
        assert_true(change.value == "r1c2_v2", "Compress Changes: record1 col2 latest value mismatch.");
      } else if (change.record_id == "record2" && change.col_name == "col1") {
        assert_true(change.value == "r2c1_v2", "Compress Changes: record2 col1 latest value mismatch.");
      } else {
        assert_true(false, "Compress Changes: Unexpected change present.");
      }
    }
    std::cout << "Test 'Compression Order Verification' passed." << std::endl;
  }

  // Test Case 11: Compression with Only Deletions
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Multiple deletions on different records
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", std::nullopt, std::nullopt, 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", std::nullopt, std::nullopt, 1, 2, 1));
    // Redundant deletions
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", std::nullopt, std::nullopt, 2, 3, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", std::nullopt, std::nullopt, 2, 4, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, __deleted__ with latest version
    // - record2, __deleted__ with latest version
    assert_true(changes.size() == 2, "Compress Changes: Only latest deletions per record should remain.");
    for (const auto &change : changes) {
      assert_true(!change.col_name.has_value(), "Compress Changes: Deletion should have no column name.");
      assert_true(!change.value.has_value(), "Compress Changes: Deletion should have no value.");
      if (change.record_id == "record1") {
        assert_true(change.col_version == 2, "Compress Changes: record1 latest deletion version mismatch.");
      } else if (change.record_id == "record2") {
        assert_true(change.col_version == 2, "Compress Changes: record2 latest deletion version mismatch.");
      } else {
        assert_true(false, "Compress Changes: Unexpected record ID present.");
      }
    }
    std::cout << "Test 'Compression with Only Deletions' passed." << std::endl;
  }

  // // Test Case 12: Compression with Mixed Insertions and Deletions on the Same Record
  // {
  //   CrdtVector<Change<CrdtString, CrdtString>> changes;
  //   // Insertions
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "v1", 1, 1, 1));
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "v2", 1, 2, 1));
  //   // Deletion of record1
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", std::nullopt, std::nullopt, 2, 3, 1));
  //   // Re-insertion after deletion (should be treated as a new state)
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "v3", 3, 4, 1));

  //   CRDT<CrdtString, CrdtString>::compress_changes(changes);

  //   // Expected compressed changes:
  //   // - record1, __deleted__ at version 2
  //   // - record1, col1: "v3" at version 3
  //   assert_true(changes.size() == 2, "Compress Changes: Should handle mixed insertions and deletions correctly.");
  //   for (const auto &change : changes) {
  //     if (change.record_id == "record1" && !change.col_name.has_value()) {
  //       assert_true(change.col_version == 2, "Compress Changes: record1 deletion version mismatch.");
  //     } else if (change.record_id == "record1" && change.col_name == "col1") {
  //       assert_true(change.value == "v3", "Compress Changes: record1 col1 latest value mismatch.");
  //       assert_true(change.col_version == 3, "Compress Changes: record1 col1 latest version mismatch.");
  //     } else {
  //       assert_true(false, "Compress Changes: Unexpected change present.");
  //     }
  //   }
  //   std::cout << "Test 'Mixed Insertions and Deletions on the Same Record' passed." << std::endl;
  // }

  // Test Case 13: Compression with Multiple Columns and Deletions
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Record1, Column1
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "v1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "v2", 2, 2, 1));
    // Record1, Column2
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", "v3", 1, 3, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col2", std::nullopt, 2, 4, 1));
    // Record1, Column3
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col3", "v4", 1, 5, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    // Expected compressed changes:
    // - record1, col1: "v2"
    // - record1, col2: deletion
    // - record1, col3: "v4"
    assert_true(changes.size() == 3, "Compress Changes: Should correctly compress multiple columns with deletions.");
    for (const auto &change : changes) {
      if (change.record_id == "record1" && change.col_name == "col1") {
        assert_true(change.value == "v2", "Compress Changes: record1 col1 latest value mismatch.");
      } else if (change.record_id == "record1" && change.col_name == "col2") {
        assert_true(!change.value.has_value(), "Compress Changes: record1 col2 should be deleted.");
      } else if (change.record_id == "record1" && change.col_name == "col3") {
        assert_true(change.value == "v4", "Compress Changes: record1 col3 value mismatch.");
      } else {
        assert_true(false, "Compress Changes: Unexpected change present.");
      }
    }
    std::cout << "Test 'Multiple Columns with Deletions' passed." << std::endl;
  }

  // // Test Case 14: Compression with Overlapping Changes Across Records
  // {
  //   CrdtVector<Change<CrdtString, CrdtString>> changes;
  //   // Record1
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v1", 1, 1, 1));
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v2", 2, 2, 1));
  //   // Record2
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v1", 1, 3, 1));
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v2", 2, 4, 1));
  //   // Record1 Deletion
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record1", std::nullopt, std::nullopt, 3, 5, 1));
  //   // Record2 Update
  //   changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col1", "r2c1_v3", 3, 6, 1));

  //   CRDT<CrdtString, CrdtString>::compress_changes(changes);

  //   // Expected compressed changes:
  //   // - record1, __deleted__ at version 3
  //   // - record2, col1: "r2c1_v3" at version 3
  //   assert_true(changes.size() == 2, "Compress Changes: Overlapping changes across records should compress correctly.");
  //   for (const auto &change : changes) {
  //     if (change.record_id == "record1") {
  //       assert_true(!change.col_name.has_value(), "Compress Changes: record1 should be deleted.");
  //       assert_true(change.col_version == 3, "Compress Changes: record1 deletion version mismatch.");
  //     } else if (change.record_id == "record2" && change.col_name == "col1") {
  //       assert_true(change.value == "r2c1_v3", "Compress Changes: record2 col1 latest value mismatch.");
  //       assert_true(change.col_version == 3, "Compress Changes: record2 col1 latest version mismatch.");
  //     } else {
  //       assert_true(false, "Compress Changes: Unexpected change present.");
  //     }
  //   }
  //   std::cout << "Test 'Overlapping Changes Across Records' passed." << std::endl;
  // }

  // Test Case 15: Compression with Multiple Insertions and No Overwrites
  {
    CrdtVector<Change<CrdtString, CrdtString>> changes;
    // Multiple insertions on different records
    changes.emplace_back(Change<CrdtString, CrdtString>("record1", "col1", "r1c1_v1", 1, 1, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record2", "col2", "r2c2_v1", 1, 2, 1));
    changes.emplace_back(Change<CrdtString, CrdtString>("record3", "col3", "r3c3_v1", 1, 3, 1));

    CRDT<CrdtString, CrdtString>::compress_changes(changes);

    assert_true(changes.size() == 3, "Compress Changes: All distinct insertions should remain.");
    std::cout << "Test 'Multiple Insertions with No Overwrites' passed." << std::endl;
  }

  std::cout << "All tests passed successfully!" << std::endl;
  return 0;
}