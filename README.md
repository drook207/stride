# Redmine Time Tracker

A desktop application built with Qt 6.8 for tracking time spent on Redmine issues. Features automatic time tracking, offline support, and seamless integration with Redmine's REST API.

## Features

### Time Tracking

- Start/Stop/Pause functionality
- Real-time elapsed time display
- Automatic time calculation
- Pause and resume capabilities
- Visual tracking status indicators

### Redmine Integration

- Automatic synchronization with Redmine
- Create time entries directly from the app
- View and manage existing time entries
- Secure API key authentication
- Error handling and retry mechanisms

### Offline Support

- SQLite database for local storage
- Automatic sync when online
- Track time without internet connection
- Persistent history of time entries
- Sync status tracking

## Requirements

- Qt 6.8 or higher
- CMake 3.16 or higher
- C++17 compatible compiler
- SQLite

## Building

1. Clone the repository:

   ```bash
   git clone https://github.com/drook207/QRedmineClient.git
   cd QRedmineClient
   ```

1. Create build directory:

   ```bash
   mkdir build
   cd build
   ```

1. Configure and build:

   ```bash
   cmake ..
   cmake --build .
   ```

## Configuration

1. Launch the application
2. Enter your Redmine URL in the settings
3. Add your API key from Redmine
4. (Optional) Configure default activity types

## Usage

1. Enter an issue ID
2. Click "Start Tracking" to begin timing
3. Use Pause/Resume as needed
4. Add comments about the work done
5. Stop tracking when finished
6. Time entry will be saved locally and synced to Redmine

## Database Schema

Time entries are stored in SQLite with the following structure:

- `id`: Primary key
- `issue_id`: Redmine issue identifier
- `hours`: Time spent in decimal hours
- `comments`: Work description
- `spent_on`: Date/time of work
- `sync_status`: Synchronization state
- `redmine_id`: ID from Redmine after sync
- `created_at`: Local creation timestamp
- `updated_at`: Last modification timestamp

## Test Environment

A disposable local Redmine instance (with demo projects/issues/users/time
entries) for manual testing is available via Docker - see
[docker/README.md](docker/README.md).

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.
