Feature: Bulk Import and Multiselect Delete Datasets

    Scenario: Opening Management Tab
        Given I am on Hootenanny
        And I click Get Started
        Then I select the "sprocket" div

    Scenario: Bulk Import Dataset
        When I click on the "Datasets" option in the "settingsSidebar"
        And I press "Import Multiple Datasets"
        Then I press "big.loud" span with text "Add Row"
        Then I select in row 0 the "File (osm,osm.zip,pbf)" option in the "Select Import Type" combobox
        And I select in row 0 the "/test-files/dcpoi_clip.osm" dataset
        And I should see row 0 input "Save As" with value "dcpoi_clip"
        Then I fill row 0 input "Save As" with value "dcpoi_clip_bulkImport_Cucumber"
        Then I select in row 1 the "File (osm,osm.zip,pbf)" option in the "Select Import Type" combobox
        And I select in row 1 the "/test-files/mapcruzinpoi_clip.osm" dataset
        And I should see row 1 input "Save As" with value "mapcruzinpoi_clip"
        Then I fill row 1 input "Save As" with value "mapcruzinpoi_clip_bulkImport_Cucumber"
        Then I press "big.loud" span with text "Import"
        And I should see "Initializing...Starting bulk import process..."
        Then I wait 2 "minutes" to see "dcpoi_clip_bulkImport_Cucumber"
        And I wait 2 "minutes" to see "mapcruzinpoi_clip_bulkImport_Cucumber"

    Scenario: Bulk Import Dataset with custom suffix
        When I click on the "Datasets" option in the "settingsSidebar"
        And I press "Import Multiple Datasets"
        Then I press "big.loud" span with text "Add Row"
        Then I select in row 0 the "File (osm,osm.zip,pbf)" option in the "Select Import Type" combobox
        And I select in row 0 the "/test-files/dcpoi_clip.osm" dataset
        And I should see row 0 input "Save As" with value "dcpoi_clip"
        Then I fill row 0 input "Save As" with value "dcpoi_clip"
        Then I select in row 1 the "File (osm,osm.zip,pbf)" option in the "Select Import Type" combobox
        And I select in row 1 the "/test-files/mapcruzinpoi_clip.osm" dataset
        And I should see row 1 input "Save As" with value "mapcruzinpoi_clip"
        Then I fill row 1 input "Save As" with value "mapcruzinpoi_clip"
        Then I fill "#customSuffix" with "_customSuffix"
        Then I press "big.loud" span with text "Import"
        And I should see "Initializing...Starting bulk import process..."
        Then I wait 2 "minutes" to see "dcpoi_clip_customSuffix"
        And I wait 2 "minutes" to see "mapcruzinpoi_clip_customSuffix"

    Scenario: Multiselect Delete
        When I click the "dcpoi_clip_bulkImport" Dataset and the "mapcruzinpoi_clip_bulkImport_Cucumber" Dataset
        And I context click the "mapcruzinpoi_clip_bulkImport_Cucumber" Dataset
        And I click the "Delete" context menu item
        And I wait
        And I accept the alert
        Then I wait 60 "seconds" to not see "dcpoi_clip_bulkImport_Cucumber"
        Then I wait 60 "seconds" to not see "mapcruzinpoi_clip_bulkImport_Cucumber"
        When I click the "dcpoi_clip_customSuffix" Dataset and the "mapcruzinpoi_clip_customSuffix" Dataset
        And I context click the "mapcruzinpoi_clip_customSuffix" Dataset
        And I click the "Delete" context menu item
        And I wait
        And I accept the alert
        Then I wait 60 "seconds" to not see "dcpoi_clip_customSuffix"
        Then I wait 60 "seconds" to not see "mapcruzinpoi_clip_customSuffix"

