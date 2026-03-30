#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <map>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <functional>
#include <optional>
#include <numeric>
#include <limits>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace util {

// Simple string hash (not cryptographic — demonstration only)
std::size_t hashPassword(const std::string& pw) {
    return std::hash<std::string>{}(pw);
}

std::string formatCurrency(double amount) {
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

std::string formatDate(std::time_t t) {
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return std::string(buf);
}

long generateAccountNumber() {
    static long next = 100000001;
    return next++;
}

void printLine(int width = 60) {
    std::cout << std::string(width, '-') << "\n";
}

void printHeader(const std::string& title, int width = 60) {
    printLine(width);
    int pad = (width - static_cast<int>(title.size()) - 2) / 2;
    std::cout << "|" << std::string(pad, ' ') << title
              << std::string(width - pad - static_cast<int>(title.size()) - 2, ' ') << "|\n";
    printLine(width);
}

void clearInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

double readPositiveDouble(const std::string& prompt) {
    double val = -1;
    while (val <= 0) {
        std::cout << prompt;
        if (!(std::cin >> val) || val <= 0) {
            std::cout << "  Please enter a positive number.\n";
            clearInput();
            val = -1;
        }
    }
    clearInput();
    return val;
}

int readMenuChoice(int lo, int hi) {
    int choice = -1;
    while (choice < lo || choice > hi) {
        std::cout << "\n  Choice: ";
        if (!(std::cin >> choice) || choice < lo || choice > hi) {
            std::cout << "  Enter a number between " << lo << " and " << hi << ".\n";
            clearInput();
            choice = -1;
        }
    }
    clearInput();
    return choice;
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

} // namespace util

// ─────────────────────────────────────────────────────────────────────────────
// Transaction
// ─────────────────────────────────────────────────────────────────────────────

struct Transaction {
    enum class Type { Deposit, Withdrawal, Transfer, Received, Loan, LoanRepayment,
                      InterestCharge, FeeCharge, Reversal, Scheduled };

    static std::string typeName(Type t) {
        switch (t) {
            case Type::Deposit:        return "Deposit";
            case Type::Withdrawal:     return "Withdrawal";
            case Type::Transfer:       return "Transfer Out";
            case Type::Received:       return "Transfer In";
            case Type::Loan:           return "Loan Disbursement";
            case Type::LoanRepayment:  return "Loan Repayment";
            case Type::InterestCharge: return "Interest Charge";
            case Type::FeeCharge:      return "Fee";
            case Type::Reversal:       return "Reversal";
            case Type::Scheduled:      return "Scheduled";
            default:                   return "Unknown";
        }
    }

    Type        type;
    double      amount;
    double      balanceAfter;
    std::string currency;
    std::string category;
    std::time_t timestamp;
    std::string note;

    Transaction(Type t, double amt, double bal,
                std::string cur = "USD", std::string cat = "General", std::string n = "")
        : type(t), amount(amt), balanceAfter(bal), currency(std::move(cur)),
          category(std::move(cat)), note(std::move(n)) {
        timestamp = std::time(nullptr);
    }

    void print(int index) const {
        std::cout << "  [" << std::setw(3) << index << "] "
                  << std::setw(16) << std::left << typeName(type)
                  << std::setw(12) << std::right << util::formatCurrency(amount)
                  << "  -> " << std::setw(12) << util::formatCurrency(balanceAfter)
                  << "  " << currency
                  << "  " << util::formatDate(timestamp);
        if (!note.empty()) std::cout << "  (" << note << ")";
        std::cout << "\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BankAccount
// ─────────────────────────────────────────────────────────────────────────────

class BankAccount {
public:
    // ── Construction ────────────────────────────────────────────────────────
    BankAccount(std::string holder, double initialBalance,
                std::string user, const std::string& pass,
                double interestRate = 0.0, double overdraftLimit = 0.0,
                double dailyLimit = 10000.0, double withdrawFee = 2.50)
        : holder_(std::move(holder)),
          accountNumber_(util::generateAccountNumber()),
          balance_(initialBalance),
          interestRate_(interestRate),
          overdraftLimit_(overdraftLimit),
          frozen_(false),
          locked_(false),
          username_(std::move(user)),
          passwordHash_(util::hashPassword(pass)),
          dailyLimit_(dailyLimit),
          loanBalance_(0.0),
          loanRate_(0.0),
          alias_(holder_),
          withdrawFee_(withdrawFee),
          dailySpent_(0.0) {
        currencies_["USD"] = initialBalance;
    }

    // ── Authentication ──────────────────────────────────────────────────────
    bool authenticate(const std::string& user, const std::string& pass) const {
        return username_ == user && passwordHash_ == util::hashPassword(pass);
    }

    bool changePassword(const std::string& oldPass, const std::string& newPass) {
        if (util::hashPassword(oldPass) != passwordHash_) {
            std::cout << "  Incorrect current password.\n";
            return false;
        }
        if (newPass.size() < 6) {
            std::cout << "  Password must be at least 6 characters.\n";
            return false;
        }
        passwordHash_ = util::hashPassword(newPass);
        std::cout << "  Password changed successfully.\n";
        return true;
    }

    // ── Deposit ─────────────────────────────────────────────────────────────
    bool deposit(double amount, const std::string& currency = "USD") {
        if (!preflight("Deposit")) return false;
        if (amount <= 0) { std::cout << "  Amount must be positive.\n"; return false; }

        balance_ += amount;
        currencies_[currency] += amount;
        log(Transaction::Type::Deposit, amount, currency);
        notify("Deposit", amount, currency);
        return true;
    }

    // ── Withdraw ────────────────────────────────────────────────────────────
    bool withdraw(double amount, const std::string& currency = "USD") {
        if (!preflight("Withdrawal")) return false;
        if (amount <= 0) { std::cout << "  Amount must be positive.\n"; return false; }

        double total = amount + withdrawFee_;
        if (balance_ + overdraftLimit_ < total) {
            std::cout << "  Insufficient funds (incl. " << util::formatCurrency(withdrawFee_) << " fee).\n";
            return false;
        }
        if (dailySpent_ + total > dailyLimit_) {
            std::cout << "  Would exceed daily transaction limit of " << util::formatCurrency(dailyLimit_) << ".\n";
            return false;
        }

        balance_ -= total;
        currencies_[currency] -= total;
        dailySpent_ += total;
        log(Transaction::Type::Withdrawal, amount, currency, "General", "incl. " + util::formatCurrency(withdrawFee_) + " fee");
        if (withdrawFee_ > 0)
            log(Transaction::Type::FeeCharge, withdrawFee_, currency, "Fee");
        notify("Withdrawal", amount, currency);
        return true;
    }

    // ── Transfer ────────────────────────────────────────────────────────────
    bool transferTo(BankAccount& recipient, double amount) {
        if (!preflight("Transfer")) return false;
        if (recipient.frozen_ || recipient.locked_) {
            std::cout << "  Recipient account is frozen or locked.\n";
            return false;
        }
        if (amount <= 0) { std::cout << "  Amount must be positive.\n"; return false; }
        if (balance_ < amount) {
            std::cout << "  Insufficient balance for transfer.\n";
            return false;
        }
        if (dailySpent_ + amount > dailyLimit_) {
            std::cout << "  Would exceed your daily limit of " << util::formatCurrency(dailyLimit_) << ".\n";
            return false;
        }

        balance_ -= amount;
        currencies_["USD"] -= amount;
        dailySpent_ += amount;
        log(Transaction::Type::Transfer, amount, "USD", "Transfer", "to #" + std::to_string(recipient.accountNumber_));

        recipient.balance_ += amount;
        recipient.currencies_["USD"] += amount;
        recipient.history_.emplace_back(Transaction::Type::Received, amount, recipient.balance_,
                                        "USD", "Transfer", "from #" + std::to_string(accountNumber_));

        std::cout << "  Transferred " << util::formatCurrency(amount)
                  << " to " << recipient.holder_ << " (#" << recipient.accountNumber_ << ").\n";
        notify("Transfer", amount, "USD");
        return true;
    }

    // ── Loan ────────────────────────────────────────────────────────────────
    bool requestLoan(double amount, double rate) {
        if (amount <= 0 || rate < 0) {
            std::cout << "  Invalid loan amount or rate.\n";
            return false;
        }
        if (loanBalance_ > 0) {
            std::cout << "  You already have an outstanding loan of " << util::formatCurrency(loanBalance_) << ".\n";
            return false;
        }
        loanBalance_ = amount;
        loanRate_ = rate;
        balance_ += amount;
        currencies_["USD"] += amount;
        log(Transaction::Type::Loan, amount, "USD", "Loan");
        std::cout << "  Loan of " << util::formatCurrency(amount)
                  << " approved at " << rate << "% interest.\n";
        return true;
    }

    void applyLoanInterest() {
        if (loanBalance_ <= 0) {
            std::cout << "  No outstanding loan.\n";
            return;
        }
        double interest = loanBalance_ * (loanRate_ / 100.0);
        loanBalance_ += interest;
        // Interest is a charge — it increases debt, does NOT add to balance
        log(Transaction::Type::InterestCharge, interest, "USD", "Loan",
            "loan balance now " + util::formatCurrency(loanBalance_));
        std::cout << "  Interest of " << util::formatCurrency(interest)
                  << " applied. Outstanding loan: " << util::formatCurrency(loanBalance_) << "\n";
    }

    bool repayLoan(double amount) {
        if (!preflight("Loan Repayment")) return false;
        if (amount <= 0) { std::cout << "  Amount must be positive.\n"; return false; }
        if (loanBalance_ <= 0) { std::cout << "  No outstanding loan.\n"; return false; }
        amount = std::min(amount, loanBalance_); // Can't overpay
        if (balance_ < amount) {
            std::cout << "  Insufficient balance to repay.\n";
            return false;
        }
        balance_ -= amount;
        currencies_["USD"] -= amount;
        loanBalance_ -= amount;
        log(Transaction::Type::LoanRepayment, amount, "USD", "Loan",
            "remaining: " + util::formatCurrency(loanBalance_));
        std::cout << "  Repaid " << util::formatCurrency(amount)
                  << ". Remaining loan: " << util::formatCurrency(loanBalance_) << "\n";
        return true;
    }

    // ── Transaction history ─────────────────────────────────────────────────
    void printHistory() const {
        util::printHeader("Transaction History");
        if (history_.empty()) {
            std::cout << "  No transactions yet.\n";
            util::printLine();
            return;
        }
        std::cout << "  " << std::setw(5) << std::left << "Idx"
                  << std::setw(16) << "Type"
                  << std::setw(12) << std::right << "Amount"
                  << "  -> " << std::setw(12) << "Balance"
                  << "  Cur   Date\n";
        util::printLine();
        for (int i = 0; i < static_cast<int>(history_.size()); ++i)
            history_[i].print(i);
        util::printLine();
    }

    void categorizeTransaction(int idx, const std::string& cat) {
        if (idx < 0 || idx >= static_cast<int>(history_.size())) {
            std::cout << "  Invalid index.\n";
            return;
        }
        history_[idx].category = cat;
        std::cout << "  Transaction [" << idx << "] categorized as \"" << cat << "\".\n";
    }

    bool reverseTransaction(int idx) {
        if (idx < 0 || idx >= static_cast<int>(history_.size())) {
            std::cout << "  Invalid index.\n";
            return false;
        }
        auto& tx = history_[idx];
        if (tx.type == Transaction::Type::Deposit) {
            balance_ -= tx.amount;
        } else if (tx.type == Transaction::Type::Withdrawal) {
            balance_ += tx.amount;
        } else {
            std::cout << "  Only deposits and withdrawals can be reversed.\n";
            return false;
        }
        double reversed = tx.amount;
        history_.erase(history_.begin() + idx);
        log(Transaction::Type::Reversal, reversed, "USD", "Reversal");
        std::cout << "  Transaction reversed. Balance: " << util::formatCurrency(balance_) << "\n";
        return true;
    }

    // ── Account controls ────────────────────────────────────────────────────
    void freeze()   { frozen_ = true;  std::cout << "  Account frozen.\n"; }
    void unfreeze() { frozen_ = false; std::cout << "  Account unfrozen.\n"; }
    void lock()     { locked_ = true;  std::cout << "  Account locked.\n"; }
    void unlock()   { locked_ = false; std::cout << "  Account unlocked.\n"; }

    void setDailyLimit(double limit) {
        dailyLimit_ = limit;
        std::cout << "  Daily limit set to " << util::formatCurrency(limit) << "\n";
    }

    void setAlias(const std::string& a) {
        alias_ = a;
        std::cout << "  Alias set to \"" << alias_ << "\"\n";
    }

    void resetDailySpent() { dailySpent_ = 0; }

    // ── Reporting ───────────────────────────────────────────────────────────
    void printStatement() const {
        util::printHeader("Account Statement");
        std::cout << "  Account Holder : " << holder_ << "\n"
                  << "  Alias          : " << alias_ << "\n"
                  << "  Account Number : " << accountNumber_ << "\n"
                  << "  Balance        : " << util::formatCurrency(balance_) << "\n"
                  << "  Overdraft Limit: " << util::formatCurrency(overdraftLimit_) << "\n"
                  << "  Interest Rate  : " << interestRate_ << "%\n"
                  << "  Daily Limit    : " << util::formatCurrency(dailyLimit_) << "\n"
                  << "  Daily Spent    : " << util::formatCurrency(dailySpent_) << "\n"
                  << "  Withdraw Fee   : " << util::formatCurrency(withdrawFee_) << "\n"
                  << "  Status         : " << (frozen_ ? "FROZEN" : locked_ ? "LOCKED" : "Active") << "\n";
        if (loanBalance_ > 0)
            std::cout << "  Loan Balance   : " << util::formatCurrency(loanBalance_)
                      << " @ " << loanRate_ << "%\n";
        util::printLine();
        std::cout << "  Currency Balances:\n";
        for (const auto& [cur, amt] : currencies_)
            std::cout << "    " << cur << ": " << util::formatCurrency(amt) << "\n";
        util::printLine();
        std::cout << "  Transactions: " << history_.size() << "\n";
        util::printLine();
    }

    void printSummary() const {
        std::cout << "  " << std::setw(12) << accountNumber_
                  << "  " << std::setw(20) << std::left << holder_
                  << std::setw(14) << std::right << util::formatCurrency(balance_)
                  << "  " << (frozen_ ? "FROZEN" : locked_ ? "LOCKED" : "Active") << "\n";
    }

    // ── Accessors ───────────────────────────────────────────────────────────
    long          number()   const { return accountNumber_; }
    const std::string& holder()   const { return holder_; }
    double        balance()  const { return balance_; }
    double        loanBal()  const { return loanBalance_; }
    bool          isFrozen() const { return frozen_; }
    bool          isLocked() const { return locked_; }

private:
    // ── Preflight check ─────────────────────────────────────────────────────
    bool preflight(const std::string& action) const {
        if (frozen_) { std::cout << "  Account is frozen. " << action << " denied.\n"; return false; }
        if (locked_) { std::cout << "  Account is locked. " << action << " denied.\n"; return false; }
        return true;
    }

    void log(Transaction::Type t, double amount, const std::string& cur,
             const std::string& cat = "General", const std::string& note = "") {
        history_.emplace_back(t, amount, balance_, cur, cat, note);
    }

    void notify(const std::string& type, double amount, const std::string& cur) const {
        std::cout << "  [Notification] " << type << " of " << util::formatCurrency(amount)
                  << " " << cur << " processed. Balance: " << util::formatCurrency(balance_) << "\n";
    }

    std::string holder_;
    long        accountNumber_;
    double      balance_;
    double      interestRate_;
    double      overdraftLimit_;
    bool        frozen_;
    bool        locked_;
    std::string username_;
    std::size_t passwordHash_;
    double      dailyLimit_;
    double      loanBalance_;
    double      loanRate_;
    std::string alias_;
    double      withdrawFee_;
    double      dailySpent_;
    std::vector<Transaction> history_;
    std::map<std::string, double> currencies_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Interactive CLI Application
// ─────────────────────────────────────────────────────────────────────────────

class BankApp {
public:
    BankApp() = default;

    void run() {
        printBanner();
        mainMenu();
    }

private:
    std::vector<BankAccount> accounts_;
    int activeIdx_ = -1; // index of currently logged-in account

    // ── Banner ──────────────────────────────────────────────────────────────
    void printBanner() const {
        std::cout << "\n";
        util::printHeader("BankEase — Simplify Your Finances");
        std::cout << "  A professional C++ banking application\n"
                  << "  Type a menu number and press Enter\n";
        util::printLine();
    }

    // ── Main menu ───────────────────────────────────────────────────────────
    void mainMenu() {
        while (true) {
            std::cout << "\n  === Main Menu ===\n"
                      << "  [1] Create Account\n"
                      << "  [2] Log In\n"
                      << "  [3] List All Accounts\n"
                      << "  [4] Run Demo\n"
                      << "  [0] Exit\n";
            int c = util::readMenuChoice(0, 4);
            switch (c) {
                case 1: createAccount(); break;
                case 2: login(); break;
                case 3: listAccounts(); break;
                case 4: runDemo(); break;
                case 0:
                    std::cout << "\n  Thank you for using BankEase. Goodbye!\n\n";
                    return;
            }
        }
    }

    // ── Create account ──────────────────────────────────────────────────────
    void createAccount() {
        util::printHeader("Create New Account");
        std::string name = util::readLine("  Full name: ");
        std::string user = util::readLine("  Username: ");
        std::string pass = util::readLine("  Password (min 6 chars): ");
        if (pass.size() < 6) {
            std::cout << "  Password too short. Minimum 6 characters.\n";
            return;
        }
        double initial = util::readPositiveDouble("  Initial deposit ($): ");

        accounts_.emplace_back(name, initial, user, pass, 2.5, 500.0, 10000.0, 2.50);
        auto& acct = accounts_.back();
        std::cout << "\n  Account created successfully!\n"
                  << "  Account Number: " << acct.number() << "\n"
                  << "  Balance: " << util::formatCurrency(acct.balance()) << "\n";
    }

    // ── Login ───────────────────────────────────────────────────────────────
    void login() {
        util::printHeader("Log In");
        std::string user = util::readLine("  Username: ");
        std::string pass = util::readLine("  Password: ");

        for (int i = 0; i < static_cast<int>(accounts_.size()); ++i) {
            if (accounts_[i].authenticate(user, pass)) {
                activeIdx_ = i;
                std::cout << "\n  Welcome back, " << accounts_[i].holder() << "!\n";
                accountMenu();
                activeIdx_ = -1;
                return;
            }
        }
        std::cout << "  Invalid credentials.\n";
    }

    // ── Account menu ────────────────────────────────────────────────────────
    void accountMenu() {
        auto& acct = accounts_[activeIdx_];
        while (true) {
            std::cout << "\n  === Account Menu (" << acct.holder() << " #" << acct.number() << ") ===\n"
                      << "  Balance: " << util::formatCurrency(acct.balance()) << "\n\n"
                      << "  [1]  Deposit\n"
                      << "  [2]  Withdraw\n"
                      << "  [3]  Transfer\n"
                      << "  [4]  Check Balance\n"
                      << "  [5]  Transaction History\n"
                      << "  [6]  Account Statement\n"
                      << "  [7]  Loan Menu\n"
                      << "  [8]  Account Settings\n"
                      << "  [9]  Categorize Transaction\n"
                      << "  [10] Reverse Transaction\n"
                      << "  [0]  Log Out\n";
            int c = util::readMenuChoice(0, 10);
            switch (c) {
                case 1: menuDeposit(acct); break;
                case 2: menuWithdraw(acct); break;
                case 3: menuTransfer(acct); break;
                case 4:
                    std::cout << "  Balance: " << util::formatCurrency(acct.balance()) << "\n";
                    break;
                case 5: acct.printHistory(); break;
                case 6: acct.printStatement(); break;
                case 7: loanMenu(acct); break;
                case 8: settingsMenu(acct); break;
                case 9: menuCategorize(acct); break;
                case 10: menuReverse(acct); break;
                case 0:
                    std::cout << "  Logged out.\n";
                    return;
            }
        }
    }

    // ── Deposit ─────────────────────────────────────────────────────────────
    void menuDeposit(BankAccount& acct) {
        double amt = util::readPositiveDouble("  Deposit amount ($): ");
        std::string cur = util::readLine("  Currency (default USD): ");
        if (cur.empty()) cur = "USD";
        acct.deposit(amt, cur);
    }

    // ── Withdraw ────────────────────────────────────────────────────────────
    void menuWithdraw(BankAccount& acct) {
        double amt = util::readPositiveDouble("  Withdrawal amount ($): ");
        acct.withdraw(amt);
    }

    // ── Transfer ────────────────────────────────────────────────────────────
    void menuTransfer(BankAccount& acct) {
        if (accounts_.size() < 2) {
            std::cout << "  No other accounts to transfer to.\n";
            return;
        }
        util::printHeader("Available Recipients");
        for (int i = 0; i < static_cast<int>(accounts_.size()); ++i) {
            if (i == activeIdx_) continue;
            std::cout << "  [" << i << "] " << accounts_[i].holder()
                      << " (#" << accounts_[i].number() << ")\n";
        }
        std::cout << "  Recipient index: ";
        int idx;
        std::cin >> idx;
        util::clearInput();
        if (idx < 0 || idx >= static_cast<int>(accounts_.size()) || idx == activeIdx_) {
            std::cout << "  Invalid recipient.\n";
            return;
        }
        double amt = util::readPositiveDouble("  Transfer amount ($): ");
        acct.transferTo(accounts_[idx], amt);
    }

    // ── Loan menu ───────────────────────────────────────────────────────────
    void loanMenu(BankAccount& acct) {
        while (true) {
            std::cout << "\n  === Loan Menu ===\n"
                      << "  Outstanding: " << util::formatCurrency(acct.loanBal()) << "\n\n"
                      << "  [1] Request Loan\n"
                      << "  [2] Repay Loan\n"
                      << "  [3] Apply Interest\n"
                      << "  [0] Back\n";
            int c = util::readMenuChoice(0, 3);
            switch (c) {
                case 1: {
                    double amt = util::readPositiveDouble("  Loan amount ($): ");
                    double rate = util::readPositiveDouble("  Interest rate (%): ");
                    acct.requestLoan(amt, rate);
                    break;
                }
                case 2: {
                    double amt = util::readPositiveDouble("  Repayment amount ($): ");
                    acct.repayLoan(amt);
                    break;
                }
                case 3: acct.applyLoanInterest(); break;
                case 0: return;
            }
        }
    }

    // ── Settings menu ───────────────────────────────────────────────────────
    void settingsMenu(BankAccount& acct) {
        while (true) {
            std::cout << "\n  === Account Settings ===\n"
                      << "  [1] Set Alias\n"
                      << "  [2] Change Password\n"
                      << "  [3] Set Daily Limit\n"
                      << "  [4] Freeze Account\n"
                      << "  [5] Unfreeze Account\n"
                      << "  [6] Lock Account\n"
                      << "  [7] Unlock Account\n"
                      << "  [0] Back\n";
            int c = util::readMenuChoice(0, 7);
            switch (c) {
                case 1: acct.setAlias(util::readLine("  New alias: ")); break;
                case 2: {
                    std::string old = util::readLine("  Current password: ");
                    std::string np  = util::readLine("  New password: ");
                    acct.changePassword(old, np);
                    break;
                }
                case 3: acct.setDailyLimit(util::readPositiveDouble("  New daily limit ($): ")); break;
                case 4: acct.freeze(); break;
                case 5: acct.unfreeze(); break;
                case 6: acct.lock(); break;
                case 7: acct.unlock(); break;
                case 0: return;
            }
        }
    }

    // ── Categorize ──────────────────────────────────────────────────────────
    void menuCategorize(BankAccount& acct) {
        acct.printHistory();
        std::cout << "  Transaction index: ";
        int idx;
        std::cin >> idx;
        util::clearInput();
        std::string cat = util::readLine("  Category: ");
        acct.categorizeTransaction(idx, cat);
    }

    // ── Reverse ─────────────────────────────────────────────────────────────
    void menuReverse(BankAccount& acct) {
        acct.printHistory();
        std::cout << "  Transaction index to reverse: ";
        int idx;
        std::cin >> idx;
        util::clearInput();
        acct.reverseTransaction(idx);
    }

    // ── List accounts ───────────────────────────────────────────────────────
    void listAccounts() const {
        util::printHeader("All Accounts");
        if (accounts_.empty()) {
            std::cout << "  No accounts yet.\n";
        } else {
            std::cout << "  " << std::setw(12) << std::left << "Account #"
                      << "  " << std::setw(20) << "Holder"
                      << std::setw(14) << std::right << "Balance"
                      << "  Status\n";
            util::printLine();
            for (const auto& a : accounts_) a.printSummary();
        }
        util::printLine();
    }

    // ── Demo mode ───────────────────────────────────────────────────────────
    void runDemo() {
        util::printHeader("Running Demo");
        std::cout << "\n  Creating two demo accounts...\n\n";

        accounts_.emplace_back("Alice Johnson",  5000.0, "alice", "pass123", 2.5, 500.0, 10000.0, 2.50);
        accounts_.emplace_back("Bob Smith",      2000.0, "bob",   "pass456", 2.5, 300.0, 10000.0, 2.50);
        auto& alice = accounts_[accounts_.size() - 2];
        auto& bob   = accounts_[accounts_.size() - 1];

        std::cout << "  Alice (#" << alice.number() << ") — " << util::formatCurrency(alice.balance()) << "\n";
        std::cout << "  Bob   (#" << bob.number()   << ") — " << util::formatCurrency(bob.balance()) << "\n\n";

        // Deposit
        std::cout << "  --- Alice deposits $1,500 ---\n";
        alice.deposit(1500.0);

        // Withdraw
        std::cout << "\n  --- Alice withdraws $300 ---\n";
        alice.withdraw(300.0);

        // Transfer
        std::cout << "\n  --- Alice transfers $1,000 to Bob ---\n";
        alice.transferTo(bob, 1000.0);

        // Loan
        std::cout << "\n  --- Bob requests $5,000 loan at 4.5% ---\n";
        bob.requestLoan(5000.0, 4.5);

        // Interest
        std::cout << "\n  --- Apply interest on Bob's loan ---\n";
        bob.applyLoanInterest();

        // Repay
        std::cout << "\n  --- Bob repays $2,000 ---\n";
        bob.repayLoan(2000.0);

        // Freeze & unfreeze
        std::cout << "\n  --- Freeze Alice's account, attempt deposit, then unfreeze ---\n";
        alice.freeze();
        alice.deposit(100.0); // Should fail
        alice.unfreeze();
        alice.deposit(100.0); // Should succeed

        // Statements
        std::cout << "\n";
        alice.printStatement();
        alice.printHistory();

        std::cout << "\n";
        bob.printStatement();
        bob.printHistory();

        std::cout << "\n  Demo complete. Accounts are available for interactive use.\n"
                  << "  Login as alice/pass123 or bob/pass456.\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    BankApp app;
    app.run();
    return 0;
}
