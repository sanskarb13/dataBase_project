# CSCE 4350 Gradebot project

Project for gradebot score in terminal



## Local Development

### Setup

1. **Clone the repository**:

   ```bash
   git clone https://github.com/jh125486/CSCE4350_gradebot.git
   cd CSCE4350_gradebot
   ```

2. **Set up environment variables**:

   ```bash
   # Copy the example environment file
   cp .env.example .env
   
   # Edit .env with your actual secrets
   nano .env  # or your preferred editor
   ```

3. **Initialize development environment**:


   The `make init` command installs a pre-push hook that runs tests and linting before allowing a push.

### Testing Local

**Start the server**:

```bash
# Using Makefile
make local-test

# Or manually
./bin/gradebot server --port 
```

**Test the client**:

```bash


**Run tests**:

```bash
make test          # Run all tests with race detection
make test-verbose  # Run tests with verbose output
```



## Usage


