#include <gtest/gtest.h>

#include <cpp-utils/process/pipe/PipeBuilder.h>
#include <thread>

using cpputils::process::PipeBuilder;
using cpputils::process::PipeReader;
using cpputils::process::PipeWriter;
using std::unique_ptr;
using std::make_unique;

class PipeReadWriteTest : public ::testing::Test {
public:
    PipeBuilder builder;
};

TEST_F(PipeReadWriteTest, write_then_read) {
    std::thread writeThread([this]() {
        PipeWriter writer = builder.writer();
        writer.write("Hello");
    });
    writeThread.join();

    PipeReader reader = builder.reader();
    EXPECT_EQ("Hello", reader.read());
}

TEST_F(PipeReadWriteTest, read_then_write) {
    std::thread writeThread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        PipeWriter writer = builder.writer();
        writer.write("Hello");
    });

    PipeReader reader = builder.reader();
    EXPECT_EQ("Hello", reader.read());
    writeThread.join();
}

TEST_F(PipeReadWriteTest, newline_in_message) {
    std::thread writeThread([this]() {
        PipeWriter writer = builder.writer();
        writer.write("Hello\n New line");
    });
    writeThread.join();

    PipeReader reader = builder.reader();
    EXPECT_EQ("Hello\n New line", reader.read());
}

TEST_F(PipeReadWriteTest, interprocess) {
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed.");
    }
    if (pid == 0) {
        // We're the child process. Send message.
        builder.closeReader();
        PipeWriter writer = builder.writer();
        writer.write("Hello world");
        exit(0);
    }

    // We're the parent process
    builder.closeWriter();
    PipeReader reader = builder.reader();
    EXPECT_EQ("Hello world", reader.read());
}
