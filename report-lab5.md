# JOS Lab5 Report

陈仁泽 1700012774

[TOC]

## Exercise 1

在`env_create`函数中加入如下语句即可：

```C
    if (type == ENV_TYPE_FS) 
        e->env_tf.tf_eflags |= FL_IOPL_MASK;
```

> Q: 