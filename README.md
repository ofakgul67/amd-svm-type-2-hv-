
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: var(--font-sans); color: var(--color-text-primary); }
.wrap { max-width: 720px; padding: 2rem 0 3rem; }
h1 { font-size: 22px; font-weight: 500; margin-bottom: 6px; }
h2 { font-size: 16px; font-weight: 500; margin: 2rem 0 0.6rem; }
p { font-size: 15px; line-height: 1.7; color: var(--color-text-secondary); }
.sub { font-size: 14px; color: var(--color-text-secondary); margin-bottom: 1.5rem; }
.note { font-size: 13px; color: var(--color-text-secondary); padding: 10px 14px; border-left: 2px solid var(--color-border-secondary); margin: 1.2rem 0; }
ul { list-style: none; padding: 0; margin: 0.4rem 0; }
ul li { font-size: 14px; color: var(--color-text-secondary); padding: 4px 0; padding-left: 14px; position: relative; line-height: 1.6; }
ul li::before { content: "–"; position: absolute; left: 0; color: var(--color-text-tertiary); }
pre { background: var(--color-background-secondary); border-radius: var(--border-radius-md); padding: 14px 16px; font-family: var(--font-mono); font-size: 13px; line-height: 1.8; color: var(--color-text-primary); overflow-x: auto; margin: 0.5rem 0 1rem; border: 0.5px solid var(--color-border-tertiary); }
.platform { font-size: 12px; color: var(--color-text-tertiary); margin-bottom: 4px; font-family: var(--font-mono); }
hr { border: none; border-top: 0.5px solid var(--color-border-tertiary); margin: 2rem 0; }
</style>

<div class="wrap">
  <h1>amd-svm-type-2-hv</h1>
  <p class="sub">A research hypervisor for AMD processors using SVM.</p>

  <p>Runs alongside the host OS as a Type-2 hypervisor. The main focus is exploring hardware virtualization on AMD — specifically how nested paging and VM exit handling can be used for memory introspection, control, and low-level hooking.</p>

  <div class="note">This is a research project, not for production use.</div>

  <h2>What it does</h2>
  <ul>
    <li>Virtual machine control</li>
    <li>Nested Page Table (NPT) manipulation</li>
    <li>VM-exit management and monitoring</li>
  </ul>

  <h2>Features</h2>
  <ul>
    <li>SVM initialization and control unit setup</li>
    <li>NPT creation and management</li>
    <li>Custom VM-exit handlers for critical CPU events</li>
    <li>Memory control and guest access management</li>
    <li>Basic logging for debugging and analysis</li>
    <li>Multi-vCPU support with per-logical-thread state</li>
  </ul>

  <h2>Requirements</h2>
  <ul>
    <li>AMD CPU with SVM and NPT support</li>
    <li>Windows or Linux host (kernel-mode code included)</li>
    <li>Virtualization enabled in UEFI/BIOS</li>
    <li>Administrator or root privileges</li>
    <li>C++17 compatible toolchain</li>
  </ul>

  <hr>

  <h2>Building</h2>

  <div class="platform">Windows — WDK / MSVC</div>
  <pre>git clone https://github.com/ofakgul67/amd-svm-type-2-hv-.git
cd amd-svm-type-2-hv-
# Open .sln in Visual Studio with WDK, then Build Solution</pre>

  <div class="platform">Linux — GCC / Clang</div>
  <pre>git clone https://github.com/ofakgul67/amd-svm-type-2-hv-.git
cd amd-svm-type-2-hv-
mkdir build && cd build
cmake ..
make</pre>

  <p>Adjust the build scripts as needed for your environment.</p>

  <h2>Contributing</h2>
  <p style="margin-bottom: 0.6rem;">Contributions are welcome. Since this is research-oriented, clear documentation and tested patches matter a lot. Please follow existing code style, write descriptive commit messages, and include tests for new functionality.</p>

</div>
