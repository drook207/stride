# Server-side configuration for the local Redmine test environment that Redmine's REST API can't do:
# time entry activities, custom fields, the REST API setting and per-project activity overrides.
# Run inside the Redmine container via `rails runner` - docker/seed.sh does that for you:
#
#   docker compose -f docker/docker-redmine.yml cp docker/seed_redmine_config.rb redmine:/tmp/
#   docker compose -f docker/docker-redmine.yml exec -T -e SECRET_KEY_BASE=supersecretkey redmine \
#     bin/rails runner /tmp/seed_redmine_config.rb setup
#
# Phases:
#   setup   - before docker/seed_redmine.py: enables the REST API, creates the activities and
#             custom fields, and prints the admin's API key as "ADMIN_API_KEY=<key>".
#   finish  - after docker/seed_redmine.py: applies per-project activity overrides (they need the
#             projects to exist).
#
# Idempotent: everything is looked up by name and only created or updated where it differs.

phase = ARGV.first || 'setup'

def say(message)
  puts "  #{message}"
end

# Time entry activities, in display order. The last one is deliberately inactive, so clients can be
# tested against an activity that exists but must not be offered.
ACTIVITIES = [
  { name: 'Design', active: true },
  { name: 'Development', active: true, default: true },
  { name: 'Testing', active: true },
  { name: 'Code review', active: true },
  { name: 'Documentation', active: true },
  { name: 'Meeting', active: true },
  { name: 'Support', active: true },
  { name: 'Deployment', active: true },
  { name: 'Project management', active: true },
  { name: 'Legacy maintenance', active: false }
].freeze

# Activities switched off for a single project (Administration of the project -> Time tracking).
PROJECT_ACTIVITY_OVERRIDES = {
  'customer-support' => { inactive: ['Design'] }
}.freeze

def ensure_activities
  ACTIVITIES.each_with_index do |spec, index|
    activity = TimeEntryActivity.shared.find_or_initialize_by(name: spec[:name])
    created = activity.new_record?
    activity.active = spec[:active]
    activity.is_default = spec.fetch(:default, false)
    activity.position = index + 1
    activity.save!
    say "activity '#{spec[:name]}' #{created ? 'created' : 'up to date'}#{spec[:active] ? '' : ' (inactive)'}"
  end
end

def ensure_custom_field(klass, name, attributes)
  field = klass.find_or_initialize_by(name: name)
  created = field.new_record?
  attributes.each { |key, value| field.send("#{key}=", value) }
  field.save!
  say "#{klass.name.sub('CustomField', '').downcase} custom field '#{name}' #{created ? 'created' : 'up to date'}"
  field
end

def ensure_custom_fields
  # Time entries
  ensure_custom_field(TimeEntryCustomField, 'Special',
                      field_format: 'list',
                      possible_values: ['Overtime', 'On-call', 'Travel', 'Weekend', 'Training'],
                      is_required: false, visible: true, is_filter: true,
                      description: 'Marks time that is booked under special conditions.')
  ensure_custom_field(TimeEntryCustomField, 'Billable',
                      field_format: 'bool', default_value: '1', edit_tag_style: 'check_box',
                      is_required: false, visible: true, is_filter: true,
                      description: 'Whether the time is billed to the customer.')

  # Issues, available in all projects and trackers
  issue_defaults = { is_for_all: true, visible: true, is_filter: true, trackers: Tracker.all.to_a }
  ensure_custom_field(IssueCustomField, 'Customer',
                      issue_defaults.merge(field_format: 'list',
                                           possible_values: ['ACME Corp', 'Globex Corporation', 'Initech',
                                                             'Umbrella Health', 'Internal']))
  ensure_custom_field(IssueCustomField, 'Environment',
                      issue_defaults.merge(field_format: 'list',
                                           possible_values: %w[Production Staging Development]))
  ensure_custom_field(IssueCustomField, 'Story points',
                      issue_defaults.merge(field_format: 'int'))
end

def apply_project_activity_overrides
  PROJECT_ACTIVITY_OVERRIDES.each do |identifier, spec|
    project = Project.find_by(identifier: identifier)
    unless project
      say "project '#{identifier}' not found, skipping its activity overrides (run seed_redmine.py first)"
      next
    end
    settings = spec[:inactive].each_with_object({}) do |name, hash|
      activity = TimeEntryActivity.shared.find_by!(name: name)
      # parent_id makes Redmine create a project-specific copy of the shared activity
      hash[activity.id.to_s] = { 'parent_id' => activity.id.to_s, 'active' => '0' }
    end
    project.update_or_create_time_entry_activities(settings)
    say "project '#{identifier}': deactivated #{spec[:inactive].join(', ')}"
  end
end

case phase
when 'setup'
  puts 'Configuring Redmine...'
  Setting.rest_api_enabled = '1'
  say 'REST API enabled'
  ensure_activities
  ensure_custom_fields
  admin = User.find_by!(login: 'admin')
  puts "ADMIN_API_KEY=#{admin.api_key}"
when 'finish'
  puts 'Applying per-project settings...'
  apply_project_activity_overrides
else
  abort "Unknown phase '#{phase}', expected 'setup' or 'finish'"
end
