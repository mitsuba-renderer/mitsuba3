def get_differentiable_parameters(scene):
    from mitsuba.render import DifferentiableParameters

    params = DifferentiableParameters()
    children = set()

    def traverse(path, n):
        if n in children:
            return

        path += type(n).__name__
        if hasattr(n, "id"):
            identifier = n.id()
            if len(identifier) > 0 and '[0x' not in identifier:
                path += '[id=\"%s\"]' % identifier
        path += '/'
        params.set_prefix(path)

        children.add(n)

        if hasattr(n, 'put_parameters'):
            n.put_parameters(params)

        if hasattr(n, 'children'):
            for c in n.children():
                traverse(path, c)

    traverse('/', scene)
    return params
