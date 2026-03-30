# 🏦 BankEase — Simplify Your Finances

A full-featured, interactive C++ 17 banking application with account management, inter-account transfers, loan management, transaction history, multi-currency support, and account security — all in a single file that compiles with **zero warnings**.

BankEase is a menu-driven console banking system that demonstrates real-world object-oriented design in modern C++17. Create accounts, authenticate with hashed passwords, deposit, withdraw, transfer between accounts, manage loans with compound interest, freeze or lock accounts, and generate formatted statements — all through a clean interactive CLI.

<img width="1915" alt="Screenshot 2024-05-16 at 3 30 09 PM" src="https://github.com/shuddha2021/BankEase-Simplify-Your-Finances/assets/81951239/6ed39428-e89c-4c76-893d-adb0b83fd55d">

---

## ✨ Features

### Core Banking

| Feature | Description |
|:---|:---|
| **Deposit & Withdraw** | Deposit in any currency. Withdrawals enforce configurable fees and daily spending limits |
| **Inter-Account Transfer** | Transfer funds between any two accounts with balance and limit validation |
| **Transaction History** | Timestamped, categorized, formatted log of every operation |
| **Account Statements** | Full report with holder name, alias, balances, currencies, loan status, limits, and transaction count |
| **Transaction Reversal** | Reverse deposits or withdrawals with automatic balance correction |
| **Transaction Categories** | Categorize any past transaction (e.g. "Groceries", "Utilities") |

### Loan Management

| Feature | Description |
|:---|:---|
| **Request Loan** | Borrow at a specified interest rate (one outstanding loan per account) |
| **Compound Interest** | Apply interest to the loan balance — correctly increases debt, not the account balance |
| **Loan Repayment** | Partial or full repayment with overpayment protection |

### Security

| Feature | Description |
|:---|:---|
| **Authentication** | Username + hashed password (never stored as plaintext) |
| **Password Change** | Old-password verification + minimum-length enforcement |
| **Account Freeze** | Blocks all transactions while frozen |
| **Account Lock** | Blocks all transactions while locked |
| **Daily Limits** | Configurable per-account daily transaction spending cap |
| **Withdrawal Fees** | Configurable per-account fee applied to every withdrawal |

### User Experience

| Feature | Description |
|:---|:---|
| **Interactive CLI** | Full menu-driven interface with main menu, account menu, loan menu, and settings menu |
| **Built-in Demo** | Option 4 from the main menu runs a complete walkthrough with two pre-built accounts |
| **Formatted Output** | Aligned columns, currency formatting, section headers, and clear prompts |
| **Input Validation** | Positive-number enforcement, menu bounds checking, and graceful error messages |
| **Multi-Currency** | Track balances per currency symbol |

---

## 🏗 Architecture

```
BankApp (Application Shell)
  │
  ├── mainMenu()            Create Account · Log In · List Accounts · Run Demo · Exit
  │     │
  │     └── accountMenu()   Deposit · Withdraw · Transfer · Balance · History ·
  │           │              Statement · Loans · Settings · Categorize · Reverse
  │           │
  │           ├── loanMenu()      Request · Repay · Apply Interest
  │           └── settingsMenu()  Alias · Password · Daily Limit · Freeze · Lock
  │
  ├── BankAccount            Core account logic, balance management, security
  │     ├── deposit()        withdraw()        transferTo()
  │     ├── requestLoan()    applyLoanInterest()   repayLoan()
  │     ├── freeze()         lock()            authenticate()
  │     └── preflight()      Frozen/locked guard on all mutating operations
  │
  ├── Transaction            Typed, timestamped, categorized transaction records
  │     └── Type enum: Deposit | Withdrawal | Transfer | Received | Loan |
  │                     LoanRepayment | InterestCharge | FeeCharge | Reversal
  │
  └── util::                 Formatting, input helpers, password hashing
```

### Key Design Decisions

- **Single responsibility** — `BankAccount` owns financial logic, `BankApp` owns UI and routing, `Transaction` is a value type.
- **Preflight guard** — Every mutating method calls `preflight()` first, a single point that checks frozen/locked status so no operation can accidentally bypass security.
- **Interest is a charge** — `applyLoanInterest()` increases the loan balance (debt) without adding money to the account balance.
- **Transfer validation** — The sender's balance is checked directly (`balance_ < amount`), not through a compound condition.
- **Password hashing** — Passwords are never stored as plaintext. `std::hash` is used for demonstration; production systems would use bcrypt or argon2.

---

## 🚀 Getting Started

### Prerequisites

- A C++17 compiler: GCC 7+, Clang 5+, or MSVC 2017+
- CMake 3.16+ (optional — you can compile directly)

### Build with CMake

```bash
git clone https://github.com/shuddha2021/BankEase-Simplify-Your-Finances.git
cd BankEase-Simplify-Your-Finances
mkdir build && cd build
cmake ..
cmake --build .
./bankease
```

### Build directly

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -o bankease main.cpp
./bankease
```

### Run the demo

Launch the application and select **[4] Run Demo** from the main menu. This creates two accounts (Alice & Bob), runs deposits, withdrawals, transfers, loans, interest, freeze/unfreeze, and prints full statements — all automatically.

After the demo, both accounts remain available for interactive use:
- Alice: `alice` / `pass123`
- Bob: `bob` / `pass456`

---

## 📁 Project Structure

```
BankEase-Simplify-Your-Finances/
├── main.cpp           ← entire application (BankApp, BankAccount, Transaction, util)
├── CMakeLists.txt     ← CMake build configuration (C++17, strict warnings)
├── .gitignore         ← ignores build artifacts and IDE files
└── README.md
```

Single-file architecture. No external dependencies. Compiles with `-Wall -Wextra -Wpedantic` and produces **zero warnings**.

---

## 🔧 Tech Stack

| Concern | Implementation |
|:---|:---|
| **Language** | C++17 |
| **Build System** | CMake 3.16+ or direct `g++` / `clang++` invocation |
| **IO & Formatting** | `<iostream>`, `<iomanip>`, `<sstream>` |
| **Data Structures** | `std::vector<Transaction>` for history, `std::map<string, double>` for multi-currency balances |
| **Security** | `std::hash` for password hashing (demonstration — not cryptographic) |
| **Timestamps** | `<ctime>` for transaction timestamps |
| **Warnings** | Clean under `-Wall -Wextra -Wpedantic` (GCC/Clang) and `/W4` (MSVC) |

---

## 🤝 Contributing

Contributions welcome. Some ideas for extension:

- Persistent storage (save/load accounts to file or SQLite)
- Cryptographic password hashing (bcrypt / argon2)
- Monthly statement generation with date-range filtering
- Interest accrual on savings accounts
- Unit tests with Google Test or Catch2

Fork the repo, create a feature branch, and submit a pull request.

---

## 📄 License

MIT
