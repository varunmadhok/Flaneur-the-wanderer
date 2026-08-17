# Flaneur-the-wanderer
An agent that randomly selects an article from Wikipedia paraphrases the summary in a whimsical manner for sharing on Discord. Project is written in C and uses Curl and CJSON libraries. It demonstrates the use of Discord webhooks and LLM calls from the agent.
# Why this exists
Most Discord agents assume a full OS, a language runtime, a garbage collector, and hundreds of megabytes of headroom for good measure. This one doesn't. Every buffer is static and of fixed-size - nothing is malloc'd - specifically so it can run in a RAM-constrained environment. This bot has been tested running successfully inside a Firecracker microVM on BareMetal Cloud. 
<img width="1255" height="442" alt="image" src="https://github.com/user-attachments/assets/ae706ea4-6e36-4524-af54-55fc45845921" />

# How it works
    1. Agent pulls a random wikipedia page every 1800 seconds. The delay can be updated;
    2. The wikipedia page title and extract is passed to an LLM (Groq in the program, but can be replaced), with a request for paraphrasing via "You are an eerie, poetic AI vagabond wandering through Wikipedia. Write a short, surreal, 2-sentence journal reflection about what you just discovered.");
    3. Program is written in C and uses the Curl and cJSON libraries.

# Build instructions
Update the following in flaneur.c with the requisite webhook and Groq key. 
```
#define DISCORD_WEBHOOK "INSERT_DISCORD_CHANNEL_WEBHOOK_HERE"
#define LLM_API_KEY "INSERT_GROQ_KEY_HERE" 
```
## BareMetal
```
git clone https://github.com/ReturnInfinity/BareMetal-App
cp flaneur.c BareMetal-App/
cd BareMetal-App
./setup.sh
./1-build.sh flaneur.c
./2-run.sh
./3-upload.sh # optional - upload to BareMetal Cloud
```
## *nix (Linux/BSD/macOS)
flaneur.c is also a valid standalone C program - just link it against libcurl and libcjson:
```
gcc -O2 -o flaneur flaneur.c -lcurl -lcjson
./flaneur ```
