# FEU E-Library Management System

A comprehensive library management system built with C++ and ImGui, featuring a modern UI and complete book lending workflow.

## Coded by:
- Lannce Carrillo
- Cris Justin Solomon 

## Papers by:
- Jeriko Pitao
- Akia Emric

## Installation
Download - [FEU E-LIBRARY SETUP](https://drive.google.com/file/d/1EJ0ODe2xJKDbqosSDyLZ41MF3AmxWt3a/view?usp=sharing)

## Features

### For Students
- 📚 Browse book catalog with cover images
- 🔍 Search books by title, author, or category
- 📖 Borrow and return books
- ⭐ Favorite books for quick access
- 👤 Personal profile management
- 💰 View borrowing history and fines

### For Administrators
- 📊 Complete dashboard with statistics
- 👥 User management (create, view, delete accounts)
- 📚 Book catalog management (add, remove books)
- 📋 Borrow history tracking
- ⚙️ Configurable fine system (₱ per day, max fine, loan period)
- 📄 Activity logs with PDF export
- 🔄 CSV import/export for bulk operations
  
### Recommendation
- Impliment a database for whole online system
- Firebase etc..

### Activity Logging
- Automatic tracking of all user actions
- Login/logout events
- Book borrowing and returns
- User account changes
- Administrative actions
- Filterable by user, action type, and date
- Export to PDF for reporting

## System Requirements

- Windows 10/11
- Visual Studio 2019 or later (or VS Build Tools)
- DirectX 11
- MSBuild

## Information about the CMD Pop up warning
- Batch file is used to guarantee to run the program in full admin
  because it is required for account creation,
-Only work around I found is this so if you have any recommendation then just comment.

## Installation

1. Clone this repository:
```bash
git clone https://github.com/LannceXD/FEU-E-Library-Management-System.git
cd FEU-E-Library-Management-System
```

2. Build the project:
```bash
cd examples\example_win32_directx11
build_win32.bat
```

Or use MSBuild directly:
```bash
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" example_win32_directx11.vcxproj /p:Configuration=Release /p:Platform=x64
```

3. Run the executable:
```bash
bin\x64\Release\FINALPROJECT.exe
```

## Default Credentials

**Admin Account:**
- Username: `admin`
- Password: `admin`

**Student Account:**
- Create one

## Project Structure

```
Final Project/
├── examples/
│   └── example_win32_directx11/
│       ├── main.cpp              # Main application code
│       ├── myfonts.h             # Font resources
│       ├── db/                   # Database files (CSV)
│       │   ├── users.csv
│       │   ├── usersprofile.csv
│       │   ├── booklist.csv
│       │   ├── borrow_history.csv
│       │   └── activity_logs.csv
│       └── icons/                # Book cover images
├── backends/                     # ImGui backends (Win32, DX11)
├── imgui*.cpp/h                 # ImGui library files
└── docs/                        # Documentation
```

## Fine System

The fine system is fully configurable by administrators:
- **Loan Period**: Default 14 days (adjustable 1-90 days)
- **Fine Rate**: Default ₱5/day (adjustable)
- **Maximum Fine**: Default ₱500 (adjustable)

Fines are automatically calculated when books are returned late.

## Database Format

All data is stored in CSV format for easy editing:

**users.csv**: `username,password,role`
**usersprofile.csv**: `username,name,section,gender,age`
**booklist.csv**: `title,author,category,iconPath`
**borrow_history.csv**: `bookTitle,borrowerName,borrowDate,dueDate,returnDate,isReturned,returnCode,fine,finePaid`
**activity_logs.csv**: `timestamp,username,action,details`

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE.txt file for details.

## Acknowledgments

- Built with [Dear ImGui](https://github.com/ocornut/imgui)
- Uses STB Image for image loading
- DirectX 11 for rendering

## Contact

For questions or support, please open an issue on GitHub.
=======
# FEU-E-Library-Management-System
>>>>>>> c6bb32028a3b50840676e919fb25530b3b7b803b
