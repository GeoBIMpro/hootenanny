Feature: Conflate feature with stats

  Scenario: I can conflate the AllDataTypes data
    Given I am on Hootenanny
    And I resize the window
    And I click Get Started
    And I press "Add Reference Dataset"
    And I click the "AllDataTypesACucumber" Dataset
    And I press "Add Layer"
    Then I wait 30 "seconds" to see "AllDataTypesACucumber"
    And I press "Add Secondary Dataset"
    And I click the "AllDataTypesBCucumber" Dataset
    And I press "Add Layer"
    Then I wait 30 "seconds" to see "AllDataTypesBCucumber"
    Then I should see "Conflate"
    And I press "Conflate"
    And I append "saveAs" input with "_Cucumber"
    And I select the "true" option in "#containerofisCollectStats"
    And I scroll element into view and press "conflate2"
    Then I wait 30 "seconds" to see "Conflating …"
    Then I wait 3 "minutes" to see "Merged_AllDataTypes"
    Then I click the "info" button
    When I press "Statistics"
    And I should see stats "featurepercents" "pois" "review" "68.8%"
    And I should see stats "featurecounts" "buildings" "merged" "4"
    And I should see stats "featurepercents" "roads" "unmatched" "20.0%"
    And I should see stats "featurecounts" "waterways" "review" "0"
    Then I click the "trash" button
    And I accept the alert
    And I wait 5 "seconds" to not see "Merged_AllDataTypes"