# Tests

产品静态护栏当前位于仓库根目录：

```text
/home/jason/armino1/tools/tests/test_cmaiw82al_ai_toy_static_guards.py
```

运行：

```sh
cd /home/jason/armino1
python3 -m unittest tools.tests.test_cmaiw82al_ai_toy_static_guards -v
```

这些测试用于防止把上游参考板的 GPIO、LCD、资源路径和 MIC 默认值重新带回
CMAiW82AL 产品工程。
