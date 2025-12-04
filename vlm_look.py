#!/usr/bin/env python3
"""
vlm_look.py - Visual Language Model Screen Analyzer for C64 Development

Uses a local VLM (via Ollama) to analyze VICE screenshots and provide
detailed descriptions for AI coding agents.

This gives the AI agent TRUE vision instead of ASCII art approximation.

Requirements:
    - Ollama running with a VLM model (qwen2-vl, llava, etc.)
    - VICE running with -remotemonitor
    - ollama Python library: pip install ollama

Usage:
    python3 vlm_look.py                    # Analyze current screen
    python3 vlm_look.py --model qwen2-vl   # Use specific model
    python3 vlm_look.py --prompt "..."     # Custom analysis prompt
    python3 vlm_look.py --existing img.png # Analyze existing image

Author: C64AIToolChain Project
Date: December 2024
"""

import socket
import time
import sys
import os
import argparse
from pathlib import Path

try:
    from ollama import Client
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False

try:
    from PIL import Image
    import io
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


# Configuration
OLLAMA_HOST = os.environ.get('OLLAMA_HOST', 'http://192.168.1.62:11434')
DEFAULT_MODEL = os.environ.get('OLLAMA_MODEL', 'qwen3-vl')
VICE_PORT = 6510

# Default prompt for game analysis
DEFAULT_PROMPT = """Analyze this Commodore 64 game screenshot. Describe:
1. What game elements are visible (snake, apple, walls, score, etc.)
2. The position of the main character/sprite
3. Any text displayed on screen
4. The current game state (playing, game over, title screen, etc.)
5. Any visual issues or bugs you notice

Be concise but precise about positions (top/bottom/left/right/center)."""


def take_vice_screenshot(output_path: str, timeout: float = 3.0) -> bool:
    """Take a screenshot from VICE via remote monitor."""
    output_path = os.path.abspath(output_path)
    command = f'screenshot "{output_path}" 2\n'
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('localhost', VICE_PORT))
        sock.sendall(command.encode())
        time.sleep(0.5)
        try:
            sock.recv(1024)
        except socket.timeout:
            pass
        sock.close()
        time.sleep(0.3)
        return os.path.exists(output_path)
    except socket.error as e:
        print(f"Error connecting to VICE: {e}")
        return False


def get_ollama_client(host: str = None) -> 'Client':
    """Create Ollama client with specified or default host."""
    if host is None:
        host = OLLAMA_HOST
    return Client(host=host)


def analyze_with_ollama(image_path: str, prompt: str, 
                        model: str = None, host: str = None,
                        verbose: bool = True) -> str:
    """
    Send image to Ollama VLM for analysis using the ollama library.
    
    Args:
        image_path: Path to the image file
        prompt: The analysis prompt
        model: Ollama model name (must support vision)
        host: Ollama server URL
        verbose: Print progress messages
        
    Returns:
        The model's analysis text
    """
    if not HAS_OLLAMA:
        return "Error: ollama library required. Install with: pip install ollama"
    
    if model is None:
        model = DEFAULT_MODEL
    
    if not os.path.exists(image_path):
        return f"Error: Image not found: {image_path}"
    
    try:
        client = get_ollama_client(host)
        
        if verbose:
            print(f"Sending to {model} at {host or OLLAMA_HOST} for analysis...")
        
        # Use chat with image - ollama library handles base64 encoding
        response = client.chat(
            model=model,
            messages=[
                {
                    'role': 'user',
                    'content': prompt,
                    'images': [image_path]  # ollama lib accepts file paths directly
                }
            ]
        )
        
        return response['message']['content']
        
    except Exception as e:
        error_msg = str(e)
        if "connection" in error_msg.lower() or "refused" in error_msg.lower():
            return f"Error: Cannot connect to Ollama at {host or OLLAMA_HOST}. Is Ollama running?"
        elif "not found" in error_msg.lower():
            return f"Error: Model '{model}' not found. Try: ollama pull {model}"
        else:
            return f"Error calling Ollama: {e}"


def list_ollama_models(host: str = None) -> list:
    """List available Ollama models."""
    if not HAS_OLLAMA:
        return []
    
    try:
        client = get_ollama_client(host)
        response = client.list()
        # Handle both dict and object response formats
        if hasattr(response, 'models'):
            models = response.models
            return [m.model for m in models]
        else:
            return [m['name'] for m in response.get('models', [])]
    except Exception as e:
        print(f"Error listing models: {e}")
        return []


def quick_look(question: str = None, host: str = None, model: str = None) -> str:
    """
    Quick one-liner for AI agents to "see" the game.
    
    Args:
        question: Optional specific question about the screen
        host: Ollama host URL (uses env var if None)
        model: Model name (uses env var if None)
        
    Returns:
        VLM's analysis of the current screen
        
    Example:
        >>> from vlm_look import quick_look
        >>> print(quick_look())  # General analysis
        >>> print(quick_look("Is the snake about to die?"))  # Specific question
        >>> print(quick_look(host="http://192.168.1.62:11434"))  # Remote VLM
    """
    if question:
        prompt = f"{question}\n\nContext: This is a Commodore 64 game screenshot."
    else:
        prompt = None
    
    return look_with_vlm(prompt=prompt, host=host, model=model, verbose=False)


def look_with_vlm(prompt: str = None, model: str = None, 
                  image_path: str = None, host: str = None,
                  verbose: bool = True) -> str:
    """
    Main function: Capture screen and analyze with VLM.
    
    This is the key function for AI agent integration.
    Returns a detailed description of the current game screen.
    
    Args:
        prompt: Analysis prompt (uses DEFAULT_PROMPT if None)
        model: Ollama model name (uses DEFAULT_MODEL if None)
        image_path: Existing image to analyze (captures from VICE if None)
        host: Ollama host URL (uses OLLAMA_HOST if None)
        verbose: Print progress messages
        
    Returns:
        String containing the VLM's analysis
    """
    if not HAS_OLLAMA:
        return "Error: ollama library required. Install with: pip install ollama"
    
    if prompt is None:
        prompt = DEFAULT_PROMPT
    if model is None:
        model = DEFAULT_MODEL
    
    # Get or capture image
    temp_file = False
    if image_path is None:
        image_path = '/tmp/vice_vlm_screenshot.png'
        temp_file = True
        if verbose:
            print("Capturing screenshot from VICE...")
        if not take_vice_screenshot(image_path):
            return "Error: Failed to capture screenshot. Is VICE running with -remotemonitor?"
        if verbose:
            print(f"Screenshot saved: {image_path}")
    else:
        if not os.path.exists(image_path):
            return f"Error: Image not found: {image_path}"
    
    # Analyze with VLM
    result = analyze_with_ollama(image_path, prompt, model, host, verbose)
    
    # Clean up temp file
    if temp_file:
        try:
            os.remove(image_path)
        except:
            pass
    
    return result


def main():
    parser = argparse.ArgumentParser(
        description='Analyze VICE screenshots using local VLM (Ollama)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              # Analyze current VICE screen
  %(prog)s --model llava                # Use LLaVA model
  %(prog)s --existing game.png          # Analyze existing image
  %(prog)s --prompt "Is the snake alive?"  # Custom prompt
  %(prog)s --list-models                # Show available models

Environment:
  OLLAMA_HOST    Ollama server URL (default: http://192.168.1.62:11434)
  OLLAMA_MODEL   Default model (default: qwen2-vl)

For AI Agent Integration:
  This tool provides TRUE vision capability by using a local VLM
  to analyze game screenshots, replacing ASCII art approximation
  with detailed natural language descriptions.
        """
    )
    
    parser.add_argument('--model', '-m', default=DEFAULT_MODEL,
                        help=f'Ollama VLM model (default: {DEFAULT_MODEL})')
    parser.add_argument('--prompt', '-p', default=DEFAULT_PROMPT,
                        help='Analysis prompt')
    parser.add_argument('--existing', '-e', metavar='IMAGE',
                        help='Analyze existing image instead of capturing')
    parser.add_argument('--list-models', '-l', action='store_true',
                        help='List available Ollama models')
    parser.add_argument('--host', default=OLLAMA_HOST,
                        help=f'Ollama host URL (default: {OLLAMA_HOST})')
    parser.add_argument('--save', '-s', metavar='PATH',
                        help='Save screenshot to this path')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Minimal output')
    
    args = parser.parse_args()
    
    # List models
    if args.list_models:
        models = list_ollama_models(args.host)
        if models:
            print("Available Ollama models:")
            for m in models:
                vlm_marker = " (VLM)" if any(v in m.lower() for v in ['llava', 'qwen', 'vision', 'vl']) else ""
                print(f"  - {m}{vlm_marker}")
        else:
            print(f"Could not retrieve models from {args.host}. Is Ollama running?")
        return 0
    
    # Check dependencies
    if not HAS_OLLAMA:
        print("Error: ollama library required. Install with: pip install ollama")
        return 1
    
    # Determine image path
    if args.existing:
        image_path = args.existing
    elif args.save:
        image_path = args.save
        print(f"Capturing screenshot to {image_path}...")
        if not take_vice_screenshot(image_path):
            print("Failed to capture screenshot")
            return 1
    else:
        image_path = None
    
    # Analyze
    result = look_with_vlm(
        prompt=args.prompt,
        model=args.model,
        image_path=image_path,
        host=args.host,
        verbose=not args.quiet
    )
    
    if not args.quiet:
        print("\n" + "="*60)
        print("VLM ANALYSIS")
        print("="*60)
    print(result)
    if not args.quiet:
        print("="*60 + "\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
