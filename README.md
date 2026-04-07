
<style>

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
