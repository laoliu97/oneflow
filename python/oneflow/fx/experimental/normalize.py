"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
import operator
from typing import Any, Callable, Dict, Tuple, Optional

import oneflow
import oneflow.fx
import oneflow.fx as fx
from oneflow.fx import Transformer, Proxy
from oneflow.fx.node import Argument, Target, Node, map_aggregate
from oneflow.fx.operator_schemas import (
    normalize_module,
    normalize_function,
    create_type_hint,
)

from .schema_type_annotation import AnnotateTypesWithSchema


class NormalizeArgs(Transformer):
    """
    Normalize arguments to Python targets. This means that
    `args/kwargs` will be matched up to the module/functional's
    signature and rewritten to exclusively kwargs in positional order
    if `normalize_to_only_use_kwargs` is true. Also populates default
    values. Does not support positional-only parameters or varargs
    parameters (*args, **kwargs).

    If the nodes have 'type' metadata, it will use it to disambiguate
    overloads. Otherwise, it will throw an error.

    Example usage:
        m = torchvision.models.resnet18()
        traced = oneflow.fx.symbolic_trace(m)
        traced = NormalizeArgs(traced).transform()
    """

    def __init__(
        self, module: oneflow.nn.Module, normalize_to_only_use_kwargs: bool = True
    ):
        super().__init__(module)
        self.node_map: Dict[Proxy, Node] = {}
        self.normalize_to_only_use_kwargs = normalize_to_only_use_kwargs

    def run_node(self, n: Node) -> Any:
        args, kwargs = self.fetch_args_kwargs_from_env(n)

        def get_type(arg):
            if isinstance(arg, fx.Node):
                return n.meta["type"] if "type" in n.meta else None
            return type(arg)

        arg_types = map_aggregate(n.args, get_type)
        assert isinstance(arg_types, tuple)
        arg_types = tuple([create_type_hint(i) for i in arg_types])
        kwarg_types = {k: get_type(v) for k, v in kwargs.items()}
        if n.op == "call_function":
            out = self.call_function(n.target, args, kwargs, arg_types, kwarg_types)
        else:
            out = super().run_node(n)
        if n.op != "output":
            self.node_map[out] = n
            out.node.meta = n.meta
        return out

    def call_function(
        self,
        target: Target,
        args: Tuple[Argument, ...],
        kwargs: Dict[str, Any],
        arg_types: Optional[Tuple[Any, ...]] = None,
        kwarg_types: Optional[Dict[str, Any]] = None,
    ):
        assert callable(target)
        new_args_and_kwargs = normalize_function(
            target,
            args,  # type: ignore[arg-type]
            kwargs,
            arg_types,  # type: ignore[arg-type]
            kwarg_types,
            self.normalize_to_only_use_kwargs,
        )
        if new_args_and_kwargs:
            new_args, new_kwargs = new_args_and_kwargs
            return self.tracer.create_proxy(
                "call_function", target, new_args, new_kwargs
            )
        else:
            return super().call_function(target, args, kwargs)

    def call_module(
        self, target: Target, args: Tuple[Argument, ...], kwargs: Dict[str, Any]
    ):
        assert isinstance(target, str)
        new_args_and_kwargs = normalize_module(
            self.module,
            target,
            args,  # type: ignore[arg-type]
            kwargs,
            self.normalize_to_only_use_kwargs,
        )
        if new_args_and_kwargs:
            new_args, new_kwargs = new_args_and_kwargs
            return super().call_module(target, new_args, new_kwargs)
        else:
            return super().call_module(target, args, kwargs)


class NormalizeOperators(AnnotateTypesWithSchema):
    """
    Normalize callsites that are different ways of "spelling" the same
    invocation into a single, canonical call. Currently supports:

    1. Normalize operators (e.g. operator.add) to the `oneflow` ops they
       ultimately invoke (e.g. oneflow.add) when it is possible to statically
       reason that

    Example usage:

        m = torchvision.models.resnet18()

        traced = oneflow.fx.symbolic_trace(m)

        traced = NormalizeOperators(traced).transform()
    """

    binary_magic_method_remap: Dict[
        Callable[[Any, Any], Any], Callable[[Any, Any], Any]
    ] = {
        oneflow.add: operator.add,
        oneflow.mul: operator.mul,
        oneflow.sub: operator.sub,
        oneflow.div: operator.truediv,
        oneflow.floor_divide: operator.floordiv,
        oneflow.remainder: operator.mod,
        oneflow.eq: operator.eq,
        oneflow.ne: operator.ne,
        oneflow.lt: operator.lt,
        oneflow.le: operator.le,
        oneflow.gt: operator.gt,
        oneflow.ge: operator.ge,
    }

    def call_function(
        self, target: Target, args: Tuple[Argument, ...], kwargs: Dict[str, Any]
    ):
        # Normalize operators according to the magic methods implemented on tensors here:
        # https://github.com/pytorch/pytorch/blob/28c5d90b679c6b38bf4183ec99f16d933c2f1bcd/tools/autograd/templates/python_variable_methods.cpp#L1137 # noqa: B950

        assert callable(target)

        if target in self.binary_magic_method_remap:
            if len(args) != 2:
                return super().call_function(target, args, kwargs)
            lhs, rhs = args

            return super().call_function(
                target=self.binary_magic_method_remap[target],
                args=(lhs, rhs),
                kwargs={},
            )

        return super().call_function(target, args, kwargs)
