#include "Common.h"
#include "Receiver.h"
#include "Sender.h"

int main()
{
    AssertSuccess(CoInitialize(nullptr));

    // Crete receiver/sender instances.
    auto receiver = new Receiver();
    auto sender = new Sender();

    // Start receiving/sending with the default device.
    {
        IDeckLinkInput* input;
        IDeckLinkOutput* output;
        std::tie(input, output) = Utility::RetrieveDeckLinkInputOutput();

        receiver->StartReceiving(input);
        sender->StartSending(output, receiver);

        input->Release();
        output->Release();
    }

    // Wait for user interaction.
    std::puts("Press return to stop.");
    (void)std::getchar();

    // Stop receiving/sending.
    sender->StopSending();
    receiver->StopReceiving();

    // Destroy the instances.
    receiver->Release();
    sender->Release();
    
    return 0;
}
