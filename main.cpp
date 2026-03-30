#include <algorithm>
#include <cmath>
#include <ctime>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace util {

// Simple string hash (not cryptographic — demonstration only)
std::size_t hashPassword(const std::string& pw) {
    return std::hash<std::string>{}(pw);
}

std::string formatCurrency(double amount) {
    return std::format("${:.2f}", amount);
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
    std::println("{}", std::string(width, '-'));
}

void printHeader(const std::string& title, int width = 60) {
    printLine(width);
    int pad = (width - static_cast<int>(title.size()) - 2) / 2;
    int rightPad = width - pad - static_cast<int>(title.size()) - 2;
    std::println("|{}{}{}|", std::string(pad, ' '), title, std::string(rightPad, ' '));
    printLine(width);
}

void clearInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

double readPositiveDouble(const std::string& prompt) {
    double val = -1;
    while (val <= 0) {
        std::print("{}", prompt);
        if (!(std::cin >> val) || val <= 0) {
            std::println("  Please enter a positive number.");
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
        std::print("\n  Choice: ");
        if (!(std::cin >> choice) || choice < lo || choice > hi) {
            std::println("  Enter a number between {} and {}.", lo, hi);
            clearInput();
            choice = -1;
        }
    }
    clearInput();
    return choice;
}

std::string readLine(const std::string& prompt) {
    std::print("{}", prompt);
    std::string s;
    std::getline(std::cin, s);
    return s;
}

} // namespace util

// ─────────────────────────────────────────────────────────────────────────────
// Transaction
// ─────────────────────────────────────────────────────────────────────────────

struct Transaction {
    enum class Type : std::uint8_t {
        Deposit, Withdrawal, Transfer, Received, Loan, LoanRepayment,
        InterestCharge, FeeCharge, Reversal, Scheduled
    };

    static constexpr std::string_view typeName(Type t) {
        switch (t) {
            using enum Type;
            case Deposit:        return "Deposit";
            case Withdrawal:     return "Withdrawal";
            case Transfer:       return "Transfer Out";
            case Received:       return "Transfer In";
            case Loan:           return "Loan Disbursement";
            case LoanRepayment:  return "Loan Repayment";
            case InterestCharge: return "Interest Charge";
            case FeeCharge:      return "Fee";
            case Reversal:       return "Reversal";
            case Scheduled:      return "Scheduled";
        }
        std::unreachable();
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
          category(std::move(cat)), timestamp(std::time(nullptr)),
          note(std::move(n)) {}

    void print(int index) const {
        std::print("  [{:>3}] {:<16}{:>12}  -> {:>12}  {}  {}",
                   index, typeName(type),
                   util::formatCurrency(amount), util::formatCurrency(balanceAfter),
                   currency, util::formatDate(timestamp));
        if (!note.empty()) std::print("  ({})", note);
        std::println("");
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
    [[nodiscard]] bool authenticate(const std::string& user, const std::string& pass) const {
        return username_ == user && passwordHash_ == util::hashPassword(pass);
    }

    bool changePassword(const std::string& oldPass, const std::string& newPass) {
        if (util::hashPassword(oldPass) != passwordHash_) {
            std::println("  Incorrect current password.");
            return false;
        }
        if (newPass.size() < 6) {
            std::println("  Password must be at least 6 characters.");
            return false;
        }
        passwordHash_ = util::hashPassword(newPass);
        std::println("  Password changed successfully.");
        return true;
    }

    // ── Deposit ─────────────────────────────────────────────────────────────
    auto deposit(double amount, const std::string& currency = "USD")
        -> std::expected<double, std::string>
    {
        if (auto err = preflight("Deposit")) return std::unexpected(*err);
        if (amount <= 0) return std::unexpected(std::string("Amount must be positive."));

        balance_ += amount;
        currencies_[currency] += amount;
        log(Transaction::Type::Deposit, amount, currency);
        notify("Deposit", amount, currency);
        return balance_;
    }

    // ── Withdraw ────────────────────────────────────────────────────────────
    auto withdraw(double amount, const std::string& currency = "USD")
        -> std::expected<double, std::string>
    {
        if (auto err = preflight("Withdrawal")) return std::unexpected(*err);
        if (amount <= 0) return std::unexpected(std::string("Amount must be positive."));

        double total = amount + withdrawFee_;
        if (balance_ + overdraftLimit_ < total)
            return std::unexpected(std::format("Insufficient funds (incl. {} fee).",
                                               util::formatCurrency(withdrawFee_)));
        if (dailySpent_ + total > dailyLimit_)
            return std::unexpected(std::format("Would exceed daily transaction limit of {}.",
                                               util::formatCurrency(dailyLimit_)));

        balance_ -= total;
        currencies_[currency] -= total;
        dailySpent_ += total;
        log(Transaction::Type::Withdrawal, amount, currency, "General",
            std::format("incl. {} fee", util::formatCurrency(withdrawFee_)));
        if (withdrawFee_ > 0)
            log(Transaction::Type::FeeCharge, withdrawFee_, currency, "Fee");
        notify("Withdrawal", amount, currency);
        return balance_;
    }

    // ── Transfer ────────────────────────────────────────────────────────────
    auto transferTo(BankAccount& recipient, double amount)
        -> std::expected<double, std::string>
    {
        if (auto err = preflight("Transfer")) return std::unexpected(*err);
        if (recipient.frozen_ || recipient.locked_)
            return std::unexpected(std::string("Recipient account is frozen or locked."));
        if (amount <= 0) return std::unexpected(std::string("Amount must be positive."));
        if (balance_ < amount)
            return std::unexpected(std::string("Insufficient balance for transfer."));
        if (dailySpent_ + amount > dailyLimit_)
            return std::unexpected(std::format("Would exceed your daily limit of {}.",
                                               util::formatCurrency(dailyLimit_)));

        balance_ -= amount;
        currencies_["USD"] -= amount;
        dailySpent_ += amount;
        log(Transaction::Type::Transfer, amount, "USD", "Transfer",
            std::format("to #{}", recipient.accountNumber_));

        recipient.balance_ += amount;
        recipient.currencies_["USD"] += amount;
        recipient.history_.emplace_back(Transaction::Type::Received, amount, recipient.balance_,
                                        "USD", "Transfer",
                                        std::format("from #{}", accountNumber_));

        std::println("  Transferred {} to {} (#{}).",
                     util::formatCurrency(amount), recipient.holder_, recipient.accountNumber_);
        notify("Transfer", amount, "USD");
        return balance_;
    }

    // ── Loan ────────────────────────────────────────────────────────────────
    auto requestLoan(double amount, double rate)
        -> std::expected<double, std::string>
    {
        if (amount <= 0 || rate < 0)
            return std::unexpected(std::string("Invalid loan amount or rate."));
        if (loanBalance_ > 0)
            return std::unexpected(std::format("You already have an outstanding loan of {}.",
                                               util::formatCurrency(loanBalance_)));
        loanBalance_ = amount;
        loanRate_ = rate;
        balance_ += amount;
        currencies_["USD"] += amount;
        log(Transaction::Type::Loan, amount, "USD", "Loan");
        std::println("  Loan of {} approved at {}% interest.",
                     util::formatCurrency(amount), rate);
        return balance_;
    }

    void applyLoanInterest() {
        if (loanBalance_ <= 0) {
            std::println("  No outstanding loan.");
            return;
        }
        double interest = loanBalance_ * (loanRate_ / 100.0);
        loanBalance_ += interest;
        // Interest is a charge — it increases debt, does NOT add to balance
        log(Transaction::Type::InterestCharge, interest, "USD", "Loan",
            std::format("loan balance now {}", util::formatCurrency(loanBalance_)));
        std::println("  Interest of {} applied. Outstanding loan: {}",
                     util::formatCurrency(interest), util::formatCurrency(loanBalance_));
    }

    auto repayLoan(double amount)
        -> std::expected<double, std::string>
    {
        if (auto err = preflight("Loan Repayment")) return std::unexpected(*err);
        if (amount <= 0) return std::unexpected(std::string("Amount must be positive."));
        if (loanBalance_ <= 0) return std::unexpected(std::string("No outstanding loan."));
        amount = std::min(amount, loanBalance_); // Can't overpay
        if (balance_ < amount)
            return std::unexpected(std::string("Insufficient balance to repay."));
        balance_ -= amount;
        currencies_["USD"] -= amount;
        loanBalance_ -= amount;
        log(Transaction::Type::LoanRepayment, amount, "USD", "Loan",
            std::format("remaining: {}", util::formatCurrency(loanBalance_)));
        std::println("  Repaid {}. Remaining loan: {}",
                     util::formatCurrency(amount), util::formatCurrency(loanBalance_));
        return balance_;
    }

    // ── Transaction history ─────────────────────────────────────────────────
    void printHistory() const {
        util::printHeader("Transaction History");
        if (history_.empty()) {
            std::println("  No transactions yet.");
            util::printLine();
            return;
        }
        std::println("  {:<5}{:<16}{:>12}  -> {:>12}  Cur   Date", "Idx", "Type", "Amount", "Balance");
        util::printLine();
        for (int i = 0; i < static_cast<int>(history_.size()); ++i)
            history_[i].print(i);
        util::printLine();
    }

    void categorizeTransaction(int idx, const std::string& cat) {
        if (idx < 0 || idx >= static_cast<int>(history_.size())) {
            std::println("  Invalid index.");
            return;
        }
        history_[idx].category = cat;
        std::println("  Transaction [{}] categorized as \"{}\".", idx, cat);
    }

    auto reverseTransaction(int idx)
        -> std::expected<double, std::string>
    {
        if (idx < 0 || idx >= static_cast<int>(history_.size()))
            return std::unexpected(std::string("Invalid index."));

        auto& tx = history_[idx];
        if (tx.type == Transaction::Type::Deposit) {
            balance_ -= tx.amount;
        } else if (tx.type == Transaction::Type::Withdrawal) {
            balance_ += tx.amount;
        } else {
            return std::unexpected(std::string("Only deposits and withdrawals can be reversed."));
        }
        double reversed = tx.amount;
        history_.erase(history_.begin() + idx);
        log(Transaction::Type::Reversal, reversed, "USD", "Reversal");
        std::println("  Transaction reversed. Balance: {}", util::formatCurrency(balance_));
        return balance_;
    }

    // ── Account controls ────────────────────────────────────────────────────
    void freeze()   { frozen_ = true;  std::println("  Account frozen."); }
    void unfreeze() { frozen_ = false; std::println("  Account unfrozen."); }
    void lock()     { locked_ = true;  std::println("  Account locked."); }
    void unlock()   { locked_ = false; std::println("  Account unlocked."); }

    void setDailyLimit(double limit) {
        dailyLimit_ = limit;
        std::println("  Daily limit set to {}", util::formatCurrency(limit));
    }

    void setAlias(const std::string& a) {
        alias_ = a;
        std::println("  Alias set to \"{}\"", alias_);
    }

    void resetDailySpent() { dailySpent_ = 0; }

    // ── Reporting ───────────────────────────────────────────────────────────
    void printStatement() const {
        util::printHeader("Account Statement");
        std::println("  Account Holder : {}", holder_);
        std::println("  Alias          : {}", alias_);
        std::println("  Account Number : {}", accountNumber_);
        std::println("  Balance        : {}", util::formatCurrency(balance_));
        std::println("  Overdraft Limit: {}", util::formatCurrency(overdraftLimit_));
        std::println("  Interest Rate  : {}%", interestRate_);
        std::println("  Daily Limit    : {}", util::formatCurrency(dailyLimit_));
        std::println("  Daily Spent    : {}", util::formatCurrency(dailySpent_));
        std::println("  Withdraw Fee   : {}", util::formatCurrency(withdrawFee_));
        std::println("  Status         : {}", frozen_ ? "FROZEN" : locked_ ? "LOCKED" : "Active");
        if (loanBalance_ > 0)
            std::println("  Loan Balance   : {} @ {}%",
                         util::formatCurrency(loanBalance_), loanRate_);
        util::printLine();
        std::println("  Currency Balances:");
        for (const auto& [cur, amt] : currencies_)
            std::println("    {}: {}", cur, util::formatCurrency(amt));
        util::printLine();
        std::println("  Transactions: {}", history_.size());
        util::printLine();
    }

    void printSummary() const {
        std::println("  {:>12}  {:<20}{:>14}  {}",
                     accountNumber_, holder_, util::formatCurrency(balance_),
                     frozen_ ? "FROZEN" : locked_ ? "LOCKED" : "Active");
    }

    // ── Accessors ───────────────────────────────────────────────────────────
    [[nodiscard]] long               number()   const { return accountNumber_; }
    [[nodiscard]] const std::string& holder()   const { return holder_; }
    [[nodiscard]] double             balance()  const { return balance_; }
    [[nodiscard]] double             loanBal()  const { return loanBalance_; }
    [[nodiscard]] bool               isFrozen() const { return frozen_; }
    [[nodiscard]] bool               isLocked() const { return locked_; }

private:
    // ── Preflight check — returns error string if blocked ───────────────────
    [[nodiscard]] std::optional<std::string> preflight(const std::string& action) const {
        if (frozen_) return std::format("Account is frozen. {} denied.", action);
        if (locked_) return std::format("Account is locked. {} denied.", action);
        return std::nullopt;
    }

    void log(Transaction::Type t, double amount, const std::string& cur,
             const std::string& cat = "General", const std::string& note = "") {
        history_.emplace_back(t, amount, balance_, cur, cat, note);
    }

    void notify(const std::string& type, double amount, const std::string& cur) const {
        std::println("  [Notification] {} of {} {} processed. Balance: {}",
                     type, util::formatCurrency(amount), cur, util::formatCurrency(balance_));
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
    int activeIdx_ = -1;

    // ── Helper: print error from std::expected ──────────────────────────────
    static void showError(const std::string& err) {
        std::println("  {}", err);
    }

    // ── Banner ──────────────────────────────────────────────────────────────
    void printBanner() const {
        std::println("");
        util::printHeader("BankEase — Simplify Your Finances");
        std::println("  A professional C++23 banking application");
        std::println("  Type a menu number and press Enter");
        util::printLine();
    }

    // ── Main menu ───────────────────────────────────────────────────────────
    void mainMenu() {
        while (true) {
            std::println("\n  === Main Menu ===");
            std::println("  [1] Create Account");
            std::println("  [2] Log In");
            std::println("  [3] List All Accounts");
            std::println("  [4] Run Demo");
            std::println("  [0] Exit");
            int c = util::readMenuChoice(0, 4);
            switch (c) {
                case 1: createAccount(); break;
                case 2: login(); break;
                case 3: listAccounts(); break;
                case 4: runDemo(); break;
                case 0:
                    std::println("\n  Thank you for using BankEase. Goodbye!\n");
                    return;
            }
        }
    }

    // ── Create account ──────────────────────────────────────────────────────
    void createAccount() {
        util::printHeader("Create New Account");
        auto name = util::readLine("  Full name: ");
        auto user = util::readLine("  Username: ");
        auto pass = util::readLine("  Password (min 6 chars): ");
        if (pass.size() < 6) {
            std::println("  Password too short. Minimum 6 characters.");
            return;
        }
        double initial = util::readPositiveDouble("  Initial deposit ($): ");

        accounts_.emplace_back(name, initial, user, pass, 2.5, 500.0, 10000.0, 2.50);
        const auto& acct = accounts_.back();
        std::println("\n  Account created successfully!");
        std::println("  Account Number: {}", acct.number());
        std::println("  Balance: {}", util::formatCurrency(acct.balance()));
    }

    // ── Login ───────────────────────────────────────────────────────────────
    void login() {
        util::printHeader("Log In");
        auto user = util::readLine("  Username: ");
        auto pass = util::readLine("  Password: ");

        for (int i = 0; i < static_cast<int>(accounts_.size()); ++i) {
            if (accounts_[i].authenticate(user, pass)) {
                activeIdx_ = i;
                std::println("\n  Welcome back, {}!", accounts_[i].holder());
                accountMenu();
                activeIdx_ = -1;
                return;
            }
        }
        std::println("  Invalid credentials.");
    }

    // ── Account menu ────────────────────────────────────────────────────────
    void accountMenu() {
        auto& acct = accounts_[activeIdx_];
        while (true) {
            std::println("\n  === Account Menu ({} #{}) ===", acct.holder(), acct.number());
            std::println("  Balance: {}\n", util::formatCurrency(acct.balance()));
            std::println("  [1]  Deposit");
            std::println("  [2]  Withdraw");
            std::println("  [3]  Transfer");
            std::println("  [4]  Check Balance");
            std::println("  [5]  Transaction History");
            std::println("  [6]  Account Statement");
            std::println("  [7]  Loan Menu");
            std::println("  [8]  Account Settings");
            std::println("  [9]  Categorize Transaction");
            std::println("  [10] Reverse Transaction");
            std::println("  [0]  Log Out");
            int c = util::readMenuChoice(0, 10);
            switch (c) {
                case 1: menuDeposit(acct); break;
                case 2: menuWithdraw(acct); break;
                case 3: menuTransfer(acct); break;
                case 4:
                    std::println("  Balance: {}", util::formatCurrency(acct.balance()));
                    break;
                case 5: acct.printHistory(); break;
                case 6: acct.printStatement(); break;
                case 7: loanMenu(acct); break;
                case 8: settingsMenu(acct); break;
                case 9: menuCategorize(acct); break;
                case 10: menuReverse(acct); break;
                case 0:
                    std::println("  Logged out.");
                    return;
            }
        }
    }

    // ── Deposit ─────────────────────────────────────────────────────────────
    void menuDeposit(BankAccount& acct) {
        double amt = util::readPositiveDouble("  Deposit amount ($): ");
        auto cur = util::readLine("  Currency (default USD): ");
        if (cur.empty()) cur = "USD";
        if (auto r = acct.deposit(amt, cur); !r)
            showError(r.error());
    }

    // ── Withdraw ────────────────────────────────────────────────────────────
    void menuWithdraw(BankAccount& acct) {
        double amt = util::readPositiveDouble("  Withdrawal amount ($): ");
        if (auto r = acct.withdraw(amt); !r)
            showError(r.error());
    }

    // ── Transfer ────────────────────────────────────────────────────────────
    void menuTransfer(BankAccount& acct) {
        if (accounts_.size() < 2) {
            std::println("  No other accounts to transfer to.");
            return;
        }
        util::printHeader("Available Recipients");
        for (int i = 0; i < static_cast<int>(accounts_.size()); ++i) {
            if (i == activeIdx_) continue;
            std::println("  [{}] {} (#{})", i, accounts_[i].holder(), accounts_[i].number());
        }
        std::print("  Recipient index: ");
        int idx;
        std::cin >> idx;
        util::clearInput();
        if (idx < 0 || idx >= static_cast<int>(accounts_.size()) || idx == activeIdx_) {
            std::println("  Invalid recipient.");
            return;
        }
        double amt = util::readPositiveDouble("  Transfer amount ($): ");
        if (auto r = acct.transferTo(accounts_[idx], amt); !r)
            showError(r.error());
    }

    // ── Loan menu ───────────────────────────────────────────────────────────
    void loanMenu(BankAccount& acct) {
        while (true) {
            std::println("\n  === Loan Menu ===");
            std::println("  Outstanding: {}\n", util::formatCurrency(acct.loanBal()));
            std::println("  [1] Request Loan");
            std::println("  [2] Repay Loan");
            std::println("  [3] Apply Interest");
            std::println("  [0] Back");
            int c = util::readMenuChoice(0, 3);
            switch (c) {
                case 1: {
                    double amt = util::readPositiveDouble("  Loan amount ($): ");
                    double rate = util::readPositiveDouble("  Interest rate (%): ");
                    if (auto r = acct.requestLoan(amt, rate); !r)
                        showError(r.error());
                    break;
                }
                case 2: {
                    double amt = util::readPositiveDouble("  Repayment amount ($): ");
                    if (auto r = acct.repayLoan(amt); !r)
                        showError(r.error());
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
            std::println("\n  === Account Settings ===");
            std::println("  [1] Set Alias");
            std::println("  [2] Change Password");
            std::println("  [3] Set Daily Limit");
            std::println("  [4] Freeze Account");
            std::println("  [5] Unfreeze Account");
            std::println("  [6] Lock Account");
            std::println("  [7] Unlock Account");
            std::println("  [0] Back");
            int c = util::readMenuChoice(0, 7);
            switch (c) {
                case 1: acct.setAlias(util::readLine("  New alias: ")); break;
                case 2: {
                    auto old = util::readLine("  Current password: ");
                    auto np  = util::readLine("  New password: ");
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
        std::print("  Transaction index: ");
        int idx;
        std::cin >> idx;
        util::clearInput();
        auto cat = util::readLine("  Category: ");
        acct.categorizeTransaction(idx, cat);
    }

    // ── Reverse ─────────────────────────────────────────────────────────────
    void menuReverse(BankAccount& acct) {
        acct.printHistory();
        std::print("  Transaction index to reverse: ");
        int idx;
        std::cin >> idx;
        util::clearInput();
        if (auto r = acct.reverseTransaction(idx); !r)
            showError(r.error());
    }

    // ── List accounts ───────────────────────────────────────────────────────
    void listAccounts() const {
        util::printHeader("All Accounts");
        if (accounts_.empty()) {
            std::println("  No accounts yet.");
        } else {
            std::println("  {:<12}  {:<20}{:>14}  Status", "Account #", "Holder", "Balance");
            util::printLine();
            for (const auto& a : accounts_) a.printSummary();
        }
        util::printLine();
    }

    // ── Demo mode ───────────────────────────────────────────────────────────
    void runDemo() {
        util::printHeader("Running Demo");
        std::println("\n  Creating two demo accounts...\n");

        accounts_.emplace_back("Alice Johnson",  5000.0, "alice", "pass123", 2.5, 500.0, 10000.0, 2.50);
        accounts_.emplace_back("Bob Smith",      2000.0, "bob",   "pass456", 2.5, 300.0, 10000.0, 2.50);
        auto& alice = accounts_[accounts_.size() - 2];
        auto& bob   = accounts_[accounts_.size() - 1];

        std::println("  Alice (#{}) — {}", alice.number(), util::formatCurrency(alice.balance()));
        std::println("  Bob   (#{}) — {}\n", bob.number(), util::formatCurrency(bob.balance()));

        // Deposit
        std::println("  --- Alice deposits $1,500 ---");
        alice.deposit(1500.0);

        // Withdraw
        std::println("\n  --- Alice withdraws $300 ---");
        alice.withdraw(300.0);

        // Transfer
        std::println("\n  --- Alice transfers $1,000 to Bob ---");
        alice.transferTo(bob, 1000.0);

        // Loan
        std::println("\n  --- Bob requests $5,000 loan at 4.5% ---");
        bob.requestLoan(5000.0, 4.5);

        // Interest
        std::println("\n  --- Apply interest on Bob's loan ---");
        bob.applyLoanInterest();

        // Repay
        std::println("\n  --- Bob repays $2,000 ---");
        bob.repayLoan(2000.0);

        // Freeze & unfreeze
        std::println("\n  --- Freeze Alice's account, attempt deposit, then unfreeze ---");
        alice.freeze();
        alice.deposit(100.0); // Should fail
        alice.unfreeze();
        alice.deposit(100.0); // Should succeed

        // Statements
        std::println("");
        alice.printStatement();
        alice.printHistory();

        std::println("");
        bob.printStatement();
        bob.printHistory();

        std::println("\n  Demo complete. Accounts are available for interactive use.");
        std::println("  Login as alice/pass123 or bob/pass456.");
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
